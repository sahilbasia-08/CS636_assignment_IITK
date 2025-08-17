#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <vector>
#include <atomic>
#include <iostream>
#include <random>

class ConcurrentBloomFilter {
public:
    ConcurrentBloomFilter(size_t filterSize = 1 << 24, int numHashFunctions = 3);
    
    void add(int value);
    bool contains(int value);
    
    void print();
    double calculate_false_positive_rate(int numTestSamples);
    
private:
    size_t filterSize;
    int numHashFunctions;
    std::vector<std::atomic<bool>> bloomBits;
    
    size_t computeHash(int value, int seedValue);
};

ConcurrentBloomFilter::ConcurrentBloomFilter(size_t filterSize, int numHashFunctions)
    : filterSize(filterSize), numHashFunctions(numHashFunctions), bloomBits(filterSize) {}

size_t ConcurrentBloomFilter::computeHash(int value, int seedValue) {
    return (value + seedValue+10) % filterSize;
}

void ConcurrentBloomFilter::add(int value) {
    for (int i = 0; i < numHashFunctions; ++i) {
        size_t hashPosition = computeHash(value, i);
        bloomBits[hashPosition] = true;
    }
}

bool ConcurrentBloomFilter::contains(int value) {
    for (int i = 0; i < numHashFunctions; ++i) {
        size_t hashPosition = computeHash(value, i);
        if (!bloomBits[hashPosition]) {
            return false;
        }
    }
    return true;
}

double ConcurrentBloomFilter::calculate_false_positive_rate(int numTestSamples) {
    int falsePositiveCount = 0;
    std::random_device randomDevice;
    std::mt19937 randomGenerator(randomDevice());
    std::uniform_int_distribution<> distribution(1, 1000000);
    
    for (int i = 0; i < numTestSamples; ++i) {
        int randomValue = distribution(randomGenerator);
        if (!contains(randomValue)) {
            falsePositiveCount++;
        }
    }
    
    return static_cast<double>(falsePositiveCount) / numTestSamples;
}

// void ConcurrentBloomFilter::print() {
//     for (size_t i = 0; i < filterSize; ++i) {
//         if (bloomBits[i]) {
//             std::cout << "Bit " << i << " is set.\n";
//         }
//     }
// }

#endif
