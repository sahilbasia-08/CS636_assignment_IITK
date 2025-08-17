#ifndef CONCURRENT_HASH_TABLE_H
#define CONCURRENT_HASH_TABLE_H

#include <pthread.h>
#include <iostream>
#include <vector>
#include <cassert>
#include <atomic>

using namespace std;

#define INITIAL_BUCKET_COUNT 1000
#define LOAD_FACTOR_THRESHOLD 0.75

struct KeyValue {
    uint32_t key;
    uint32_t value;
};

struct Node {
    KeyValue data;
    Node* next;
};

class ConcurrentHashTable {
private:
    Node** table;
    uint32_t bucketCount;
    pthread_mutex_t* bucketMutexes;
    atomic<int> currentSize;

    uint32_t hashFunction(uint32_t key) {
        return key % bucketCount;
    }

    void resizeTable() {
        uint32_t newBucketCount = bucketCount * 2;
        Node** newTable = new Node*[newBucketCount];
        pthread_mutex_t* newMutexes = new pthread_mutex_t[newBucketCount];

        for (uint32_t i = 0; i < newBucketCount; i++) {
            newTable[i] = nullptr;
            pthread_mutex_init(&newMutexes[i], nullptr);
        }

        for (uint32_t i = 0; i < bucketCount; i++) {
            Node* currentNode = table[i];
            while (currentNode) {
                uint32_t newIdx = currentNode->data.key % newBucketCount;
                pthread_mutex_lock(&newMutexes[newIdx]);
                
                Node* newNode = new Node{currentNode->data, newTable[newIdx]};
                newTable[newIdx] = newNode;
                
                pthread_mutex_unlock(&newMutexes[newIdx]);
                currentNode = currentNode->next;
            }
        }

        delete[] table;
        delete[] bucketMutexes;

        table = newTable;
        bucketMutexes = newMutexes;
        bucketCount = newBucketCount;
    }

public:
    ConcurrentHashTable(uint32_t initialBucketCount = INITIAL_BUCKET_COUNT)
        : bucketCount(initialBucketCount), currentSize(0) {
        table = new Node*[bucketCount];
        bucketMutexes = new pthread_mutex_t[bucketCount];

        for (uint32_t i = 0; i < bucketCount; i++) {
            table[i] = nullptr;
            pthread_mutex_init(&bucketMutexes[i], nullptr);
        }
    }

    ~ConcurrentHashTable() {
        for (uint32_t i = 0; i < bucketCount; i++) {
            Node* currentNode = table[i];
            while (currentNode) {
                Node* tempNode = currentNode;
                currentNode = currentNode->next;
                delete tempNode;
            }
        }
        delete[] table;
        delete[] bucketMutexes;
    }

    void insert(KeyValue kv) {
        if ((float)currentSize / bucketCount > LOAD_FACTOR_THRESHOLD) {
            resizeTable();
        }

        uint32_t idx = hashFunction(kv.key);
        pthread_mutex_lock(&bucketMutexes[idx]);

        Node* newNode = new Node{kv, table[idx]};
        table[idx] = newNode;

        pthread_mutex_unlock(&bucketMutexes[idx]);
        currentSize++;
    }

    bool deleteKey(uint32_t key) {
        uint32_t idx = hashFunction(key);
        pthread_mutex_lock(&bucketMutexes[idx]);

        Node* currentNode = table[idx];
        Node* prevNode = nullptr;

        while (currentNode) {
            if (currentNode->data.key == key) {
                if (prevNode) {
                    prevNode->next = currentNode->next;
                } else {
                    table[idx] = currentNode->next;
                }
                delete currentNode;
                pthread_mutex_unlock(&bucketMutexes[idx]);
                currentSize--;
                return true;
            }
            prevNode = currentNode;
            currentNode = currentNode->next;
        }

        pthread_mutex_unlock(&bucketMutexes[idx]);
        return false;
    }

    bool lookup(uint32_t key, uint32_t &value) {
        uint32_t idx = hashFunction(key);
        pthread_mutex_lock(&bucketMutexes[idx]);

        Node* currentNode = table[idx];
        while (currentNode) {
            if (currentNode->data.key == key) {
                value = currentNode->data.value;
                pthread_mutex_unlock(&bucketMutexes[idx]);
                return true;
            }
            currentNode = currentNode->next;
        }

        pthread_mutex_unlock(&bucketMutexes[idx]);
        return false;
    }

    void printTableContents() {
        for (uint32_t i = 0; i < bucketCount; i++) {
            Node* currentNode = table[i];
            while (currentNode) {
                cout << "Key: " << currentNode->data.key << " Value: " << currentNode->data.value << endl;
                currentNode = currentNode->next;
            }
        }
    }

    void runUnitTest() {
        cout << "Running unit test...\n";
        for (uint32_t i = 0; i < bucketCount; i++) {
            Node* currentNode = table[i];
            while (currentNode) {
                cout << "Key: " << currentNode->data.key << " Value: " << currentNode->data.value << endl;
                currentNode = currentNode->next;
            }
        }
    }
};

#endif
