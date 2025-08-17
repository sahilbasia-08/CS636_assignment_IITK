#include <pthread.h>
#include <unistd.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>

#include "concurrent_hash_table.h"

using namespace std;

using HR = chrono::high_resolution_clock;
using HRTimer = HR::time_point;
using namespace chrono;
using namespace filesystem;

static constexpr uint64_t SEED = 42;
static const uint32_t BUCKET_COUNT = 1000;
static constexpr uint64_t MAX_OPS = 1e+15;

uint64_t totalOperations = 1e8;
uint64_t insertPercentage = 100;
uint64_t deletePercentage = 0;
uint64_t runCount = 2;
bool isUnitTestEnabled = false;

struct ThreadArgs {
    void* data;
    uint64_t operationCount;
};

int parseArguments(char* argument) {
    string input = string(argument);
    string flag;
    uint64_t value;

    try {
        flag = input.substr(0, 4);
        string remaining = input.substr(5);
        value = stol(remaining);
    } catch (...) {
        cout << "Supported: " << endl;
        cout << "-*=[], where * is:" << endl;
        return 1;
    }

    if (flag == "-ops") {
        totalOperations = value;
    } else if (flag == "-rns") {
        runCount = value;
    } else if (flag == "-add") {
        insertPercentage = value;
    } else if (flag == "-rem") {
        deletePercentage = value;
    } else if (flag == "-test") {
        isUnitTestEnabled = true;
    } else {
        cout << "Unsupported flag:" << flag << "\n";
        return 1;
    }
    return 0;
}

void readDataFromFile(path filePath, uint64_t size, uint32_t* data) {
    FILE* file = fopen(filePath.string().c_str(), "rb");
    if (!file) {
        perror(("Unable to open file: " + filePath.string()).c_str());
        exit(EXIT_FAILURE);
    }
    int status = fread(data, sizeof(uint32_t), size, file);
    if (status == 0) {
        perror(("Unable to read the file " + filePath.string()).c_str());
        exit(EXIT_FAILURE);
    }
    fclose(file);
}

void* insertBatch(void* args) {
    ThreadArgs* data = static_cast<ThreadArgs*>(args);
    KeyValue* keyValues = static_cast<KeyValue*>(data->data);
    ConcurrentHashTable table;

    auto start = HR::now();
    for (uint64_t i = 0; i < data->operationCount; i++) {
        table.insert(keyValues[i]);
    }
    auto end = HR::now();
    return new long(duration_cast<milliseconds>(end - start).count());
}

void* deleteBatch(void* args) {
    ThreadArgs* data = static_cast<ThreadArgs*>(args);
    uint32_t* keys = static_cast<uint32_t*>(data->data);
    ConcurrentHashTable table;

    auto start = HR::now();
    for (uint64_t i = 0; i < data->operationCount; i++) {
        table.deleteKey(keys[i]);
    }
    auto end = HR::now();
    return new long(duration_cast<milliseconds>(end - start).count());
}

void* searchBatch(void* args) {
    ThreadArgs* data = static_cast<ThreadArgs*>(args);
    uint32_t* keys = static_cast<uint32_t*>(data->data);
    ConcurrentHashTable table;
    uint32_t value;

    auto start = HR::now();
    for (uint64_t i = 0; i < data->operationCount; i++) {
        table.lookup(keys[i], value);
    }
    auto end = HR::now();
    return new long(duration_cast<milliseconds>(end - start).count());
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        int error = parseArguments(argv[i]);
        if (error == 1) {
            cout << "Argument error, terminating run.\n";
            exit(EXIT_FAILURE);
        }
    }

    uint64_t addOperations = totalOperations * (insertPercentage / 100.0);
    uint64_t removeOperations = totalOperations * (deletePercentage / 100.0);
    uint64_t searchOperations = totalOperations - (addOperations + removeOperations);

    cout << "NUM OPS: " << totalOperations << " ADD: " << addOperations << " REM: " << removeOperations
         << " FIND: " << searchOperations << "\n";

    assert(addOperations > 0);

    auto* insertData = new KeyValue[addOperations];
    memset(insertData, 0, sizeof(KeyValue) * addOperations);
    auto* deleteKeys = new uint32_t[removeOperations];
    memset(deleteKeys, 0, sizeof(uint32_t) * removeOperations);
    auto* searchKeys = new uint32_t[searchOperations];
    memset(searchKeys, 0, sizeof(uint32_t) * searchOperations);

    path cwd = filesystem::current_path();
    path insertKeysPath = cwd / "random_keys_insert.bin";
    path insertValuesPath = cwd / "random_values_insert.bin";
    path deletePath = cwd / "random_keys_delete.bin";
    path searchPath = cwd / "random_keys_search.bin";

    assert(filesystem::exists(insertKeysPath));
    assert(filesystem::exists(insertValuesPath));
    assert(filesystem::exists(deletePath));
    assert(filesystem::exists(searchPath));

    auto* tempInsertKeys = new uint32_t[addOperations];
    readDataFromFile(insertKeysPath, addOperations, tempInsertKeys);
    auto* tempInsertValues = new uint32_t[addOperations];
    readDataFromFile(insertValuesPath, addOperations, tempInsertValues);
    for (int i = 0; i < addOperations; i++) {
        insertData[i].key = tempInsertKeys[i];
        insertData[i].value = tempInsertValues[i];
    }
    delete[] tempInsertKeys;
    delete[] tempInsertValues;

    if (removeOperations > 0) {
        auto* tempDeleteKeys = new uint32_t[removeOperations];
        readDataFromFile(deletePath, removeOperations, tempDeleteKeys);
        for (int i = 0; i < removeOperations; i++) {
            deleteKeys[i] = tempDeleteKeys[i];
        }
        delete[] tempDeleteKeys;
    }

    if (searchOperations > 0) {
        auto* tempSearchKeys = new uint32_t[searchOperations];
        readDataFromFile(searchPath, searchOperations, tempSearchKeys);
        for (int i = 0; i < searchOperations; i++) {
            searchKeys[i] = tempSearchKeys[i];
        }
        delete[] tempSearchKeys;
    }

    mt19937 gen(SEED);
    uniform_int_distribution<uint32_t> dist(1, totalOperations);

    int threadCounts[] = {1, 2, 4, 8, 16};

    for (int numThreads : threadCounts) {
        cout << "Running with " << numThreads << " threads...\n";

        uint64_t insertsPerThread = addOperations / numThreads;
        uint64_t deletesPerThread = removeOperations / numThreads;
        uint64_t searchesPerThread = searchOperations / numThreads;

        long totalInsertTime = 0;
        long totalDeleteTime = 0;
        long totalSearchTime = 0;

        pthread_t insertThreads[numThreads];
        for (int i = 0; i < numThreads; i++) {
            ThreadArgs data = {insertData + i * insertsPerThread, insertsPerThread};
            pthread_create(&insertThreads[i], nullptr, insertBatch, (void*)&data);
        }
        for (int i = 0; i < numThreads; i++) {
            void* result;
            pthread_join(insertThreads[i], &result);
            totalInsertTime += *(long*)result;
            delete (long*)result;
        }

        pthread_t deleteThreads[numThreads];
        for (int i = 0; i < numThreads; i++) {
            ThreadArgs data = {deleteKeys + i * deletesPerThread, deletesPerThread};
            pthread_create(&deleteThreads[i], nullptr, deleteBatch, (void*)&data);
        }
        for (int i = 0; i < numThreads; i++) {
            void* result;
            pthread_join(deleteThreads[i], &result);
            totalDeleteTime += *(long*)result;
            delete (long*)result;
        }

        pthread_t searchThreads[numThreads];
        for (int i = 0; i < numThreads; i++) {
            ThreadArgs data = {searchKeys + i * searchesPerThread, searchesPerThread};
            pthread_create(&searchThreads[i], nullptr, searchBatch, (void*)&data);
        }
        for (int i = 0; i < numThreads; i++) {
            void* result;
            pthread_join(searchThreads[i], &result);
            totalSearchTime += *(long*)result;
            delete (long*)result;
        }

        cout << "Insert time (ms): " << totalInsertTime / numThreads << "\n";
        cout << "Delete time (ms): " << totalDeleteTime / numThreads << "\n";
        cout << "Search time (ms): " << totalSearchTime / numThreads << "\n";

        sleep(1);
    }

    delete[] insertData;
    delete[] deleteKeys;
    delete[] searchKeys;

    return EXIT_SUCCESS;
}
