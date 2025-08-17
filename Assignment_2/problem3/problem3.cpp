#include <iostream>
#include <pthread.h>
#include <vector>
#include <random>
#include <chrono>
#include <cstring>
#include "bloom_filter.h"

using namespace std;

int NUM_OPERATIONS = 1000000;
double ADD_PROBABILITY = 0.5;

struct ThreadArgs {
    ConcurrentBloomFilter* bloom;
    int num_operations;
    bool is_add_operation;
};

void* add_operations(void* args) {
    ThreadArgs* threadArgs = static_cast<ThreadArgs*>(args);
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(1, 1000000);

    for (int i = 0; i < threadArgs->num_operations; ++i) {
        if (threadArgs->is_add_operation) {
            if ((rand() % 100) < ADD_PROBABILITY * 100) {
                threadArgs->bloom->add(dis(gen));
            }
        } else {
            threadArgs->bloom->contains(dis(gen));
        }
    }

    pthread_exit(nullptr);
}

void print_usage() {
    cout << "Usage: bloom_filter -ops=<num_operations> -add=<add_probability>" << endl;
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-ops=", 5) == 0) {
            NUM_OPERATIONS = stoi(argv[i] + 5);
        }
        else if (strncmp(argv[i], "-add=", 5) == 0) {
            ADD_PROBABILITY = stod(argv[i] + 5) / 100.0;
            if (ADD_PROBABILITY < 0 || ADD_PROBABILITY > 1) {
                cerr << "Error: -add probability must be between 0 and 100." << endl;
                return 1;
            }
        }
        else {
            //print_usage();
            return 1;
        }
    }

    ConcurrentBloomFilter bloomFilter;

    for (int threads = 1; threads <= 16; threads *= 2) {
        cout << "Running with " << threads << " threads...\n";
        
        auto start = chrono::high_resolution_clock::now();
        vector<pthread_t> operationThreads(threads);
        vector<ThreadArgs> threadArgs(threads);
        
        for (int i = 0; i < threads; ++i) {
            threadArgs[i].bloom = &bloomFilter;
            threadArgs[i].num_operations = NUM_OPERATIONS / threads;
            threadArgs[i].is_add_operation = true;  // Change to false if you want contains operations

            pthread_create(&operationThreads[i], nullptr, add_operations, &threadArgs[i]);
        }
        
        for (auto& t : operationThreads) {
            pthread_join(t, nullptr);
        }
        
        auto end = chrono::high_resolution_clock::now();
        chrono::duration<double> operationDuration = end - start;
        cout << "Time taken for operations: " << operationDuration.count() << " seconds\n";
        
        double falsePositiveRate = bloomFilter.calculate_false_positive_rate(NUM_OPERATIONS / 10);
        cout << "False positive rate: " << falsePositiveRate << "\n";
        // bloomFilter.print();
    }
    
    return 0;
}
