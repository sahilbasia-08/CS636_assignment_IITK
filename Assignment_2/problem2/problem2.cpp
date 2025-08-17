#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <pthread.h>
#include <vector>
#include "concurrent_queue.h"

using std::cout;
using std::endl;
using std::string;
using std::chrono::duration_cast;
using HR = std::chrono::high_resolution_clock;
using HRTimer = HR::time_point;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::filesystem::path;

static constexpr uint64_t RANDOM_SEED = 42;
uint64_t NUM_OPS = 10000;
uint64_t INSERT = 50;
uint64_t DELETE = 50;
uint64_t runs = 1;
int NUM_THREADS = 4;
bool correctness_test = false;

typedef struct {
  uint32_t key;
  uint32_t value;
} KeyValue;

struct ThreadArg {
  uint64_t startIdx;
  uint64_t endIdx;
  KeyValue* h_kvs_insert;
  uint32_t* h_keys_del;
  LockFreeQueue* queue;
  std::uniform_real_distribution<double>* dist;
  std::mt19937* gen;
  uint64_t ADD;
  uint64_t REM;
  bool verbose;
};

void* pthread_worker(void* arg) {
  ThreadArg* args = static_cast<ThreadArg*>(arg);
  for (uint64_t i = args->startIdx; i < args->endIdx; ++i) {
    double randVal = (*(args->dist))(*(args->gen));
    if (randVal < INSERT && i < args->ADD) {
      args->queue->enq(args->h_kvs_insert[i].value);
      if (args->verbose) {
        cout << "[Enq] " << args->h_kvs_insert[i].value << endl;
      }
    } else if (randVal < (INSERT + DELETE) && i < args->REM) {
      int val = args->queue->deq();
      if (args->verbose) {
        cout << "[Deq] " << val << endl;
      }
    }
  }
  pthread_exit(nullptr);
}

void read_data(path pth, uint64_t n, uint32_t* data) {
  FILE* fptr = fopen(pth.string().c_str(), "rb");
  if (!fptr) {
    perror(("Unable to open file: " + pth.string()).c_str());
    exit(EXIT_FAILURE);
  }
  if (fread(data, sizeof(uint32_t), n, fptr) == 0) {
    perror(("Unable to read file: " + pth.string()).c_str());
    exit(EXIT_FAILURE);
  }
  fclose(fptr);
}

void validFlagsDescription() {
  cout << "Usage: ./a.out -ops=<number_of_operations> -t=<num_threads> [-test]\n";
}

int parse_args(int argc, char* argv[]) {
  if (argc < 2 || argc > 4) {
    validFlagsDescription();
    return 1;
  }

  for (int i = 1; i < argc; ++i) {
    string arg = argv[i];
    if (arg.substr(0, 5) == "-ops=") {
      try {
        NUM_OPS = std::stoull(arg.substr(5));
      } catch (...) {
        cout << "Error: invalid number for -ops\n";
        return 1;
      }
    } else if (arg.substr(0, 3) == "-t=") {
      try {
        NUM_THREADS = std::stoi(arg.substr(3));
        if (NUM_THREADS <= 0) throw std::invalid_argument("Invalid");
      } catch (...) {
        cout << "Error: invalid number for -t (threads)\n";
        return 1;
      }
    } else if (arg == "-test") {
      correctness_test = true;
    } else {
      validFlagsDescription();
      return 1;
    }
  }

  return 0;
}

int main(int argc, char* argv[]) {
  if (parse_args(argc, argv) != 0) {
    return EXIT_FAILURE;
  }

  if (correctness_test) {
    cout << "\nRunning Correctness Test with " << NUM_OPS << " Operations...\n";
  }

  uint64_t ADD  = NUM_OPS * (INSERT / 100.0);
  uint64_t REM  = NUM_OPS * (DELETE / 100.0);

  auto* h_kvs_insert = new KeyValue[ADD];
  memset(h_kvs_insert, 0, sizeof(KeyValue) * ADD);

  auto* h_keys_del   = new uint32_t[REM];
  memset(h_keys_del, 0, sizeof(uint32_t) * REM);

  path cwd               = std::filesystem::current_path();
  path path_insert_values= cwd / "random_values_insert.bin";
  path path_delete       = cwd / "random_keys_delete.bin";

  auto* tmp_values_insert = new uint32_t[ADD];
  read_data(path_insert_values, ADD, tmp_values_insert);
  for (uint64_t i = 0; i < ADD; i++) {
    h_kvs_insert[i].value = tmp_values_insert[i];
  }
  delete[] tmp_values_insert;

  auto* tmp_keys_delete = new uint32_t[REM];
  read_data(path_delete, REM, tmp_keys_delete);
  for (uint64_t i = 0; i < REM; i++) {
    h_keys_del[i] = tmp_keys_delete[i];
  }
  delete[] tmp_keys_delete;

  std::mt19937 gen(RANDOM_SEED);
  std::uniform_real_distribution<double> dist(0.0, 100.0);

  LockFreeQueue lfQueue;
  auto start = HR::now();

  std::vector<pthread_t> threads(NUM_THREADS);
  std::vector<ThreadArg> args(NUM_THREADS);

  for (int t = 0; t < NUM_THREADS; t++) {
    uint64_t chunkSize = NUM_OPS / NUM_THREADS;
    uint64_t startIdx  = t * chunkSize;
    uint64_t endIdx    = (t == NUM_THREADS - 1) ? NUM_OPS : (t + 1) * chunkSize;

    args[t] = ThreadArg{
      .startIdx = startIdx,
      .endIdx = endIdx,
      .h_kvs_insert = h_kvs_insert,
      .h_keys_del = h_keys_del,
      .queue = &lfQueue,
      .dist = &dist,
      .gen = &gen,
      .ADD = ADD,
      .REM = REM,
      .verbose = correctness_test
    };

    pthread_create(&threads[t], nullptr, pthread_worker, &args[t]);
  }

  for (int t = 0; t < NUM_THREADS; t++) {
    pthread_join(threads[t], nullptr);
  }

  auto end = HR::now();
  double totalTimeMs = duration_cast<milliseconds>(end - start).count();

  cout << "\nThreads: " << NUM_THREADS;
  if (correctness_test) {
    cout << " (Correctness Test)\n";
  }
  cout << "Total time (ms) for enq/deq: " << totalTimeMs << "\n";

  delete[] h_kvs_insert;
  delete[] h_keys_del;
  return EXIT_SUCCESS;
}
