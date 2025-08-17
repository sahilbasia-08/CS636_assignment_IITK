#ifndef CONCURRENT_QUEUE_H
#define CONCURRENT_QUEUE_H

#include <atomic>
#include <cstdint>
#include <iostream>
#include <cassert>
#include <thread> // for std::this_thread::sleep_for

/**
 * 128-bit pointer+version that we can CAS atomically in one shot.
 * Typically 8 bytes for the pointer + 8 for the version = 16 bytes total.
 */
struct PtrVersion {
    void*     ptr;
    uint64_t  version;
};

/**
 * Lock-free queue node. 'next' is itself a 128-bit atomic
 * so we can do versioned CAS and avoid ABA.
 */
class LockFreeQueue {
private:
    struct Node {
        int value;
        std::atomic<PtrVersion> next;
    };

    // Head and tail also store pointer+version so we can do 128-bit CAS on them.
    std::atomic<PtrVersion> head;
    std::atomic<PtrVersion> tail;

public:
    LockFreeQueue() {
        // Make a dummy node
        Node* dummy = new Node();
        dummy->value = 0;

        // Set dummy->next to {nullptr, 0}
        PtrVersion dummyNext = {nullptr, 0};
        dummy->next.store(dummyNext, std::memory_order_relaxed);

        // Initialize head/tail -> {dummy, 0}
        PtrVersion hv = {dummy, 0};
        head.store(hv, std::memory_order_relaxed);
        tail.store(hv, std::memory_order_relaxed);
    }

    ~LockFreeQueue() {
        // In real production code, you'd need safe memory reclamation (hazard pointers, etc.)
        // For the assignment, we do a naive cleanup:
        while (deq() >= 0) { /* keep dequeuing until empty */ }
        // Finally delete the dummy node
        PtrVersion h = head.load(std::memory_order_relaxed);
        delete reinterpret_cast<Node*>(h.ptr);
    }

    /**
     * Enqueue integer v into the queue.
     */
    bool enq(int v) {
        Node* newNode = new Node();
        newNode->value = v;
        PtrVersion nullNext = {nullptr, 0};
        newNode->next.store(nullNext, std::memory_order_relaxed);

        while (true) {
            // Snapshot tail
            PtrVersion tailSnap = tail.load(std::memory_order_acquire);
            Node* tailPtr = reinterpret_cast<Node*>(tailSnap.ptr);
            PtrVersion nextSnap = tailPtr->next.load(std::memory_order_acquire);

            // If tailPtr is indeed the last node (next is null)
            if (nextSnap.ptr == nullptr) {
                // Attempt to link newNode as tailPtr->next
                PtrVersion desiredNext = {newNode, nextSnap.version + 1};

                if (compareAndSwap128(tailPtr->next, nextSnap, desiredNext)) {
                    // Successfully linked. Now move tail forward if needed
                    PtrVersion desiredTail = {newNode, tailSnap.version + 1};
                    compareAndSwap128(tail, tailSnap, desiredTail);
                    return true;
                }
                // else someone else inserted => retry
            } else {
                // tail not pointing to the last node, fix it
                PtrVersion desiredTail = {nextSnap.ptr, tailSnap.version + 1};
                compareAndSwap128(tail, tailSnap, desiredTail);
            }
        }
    }

    /**
     * Dequeue one item. Returns -1 if queue empty.
     */
    int deq() {
        while (true) {
            PtrVersion headSnap = head.load(std::memory_order_acquire);
            PtrVersion tailSnap = tail.load(std::memory_order_acquire);

            Node* headPtr = reinterpret_cast<Node*>(headSnap.ptr);
            Node* tailPtr = reinterpret_cast<Node*>(tailSnap.ptr);

            PtrVersion nextSnap = headPtr->next.load(std::memory_order_acquire);
            Node* nextNode = reinterpret_cast<Node*>(nextSnap.ptr);

            // If head == tail => maybe empty
            if (headPtr == tailPtr) {
                if (nextNode == nullptr) {
                    // Truly empty
                    return -1;
                }
                // Tail is falling behind, push it forward
                PtrVersion desiredTail = {nextNode, tailSnap.version + 1};
                compareAndSwap128(tail, tailSnap, desiredTail);
            } else {
                // There's a real node to remove
                if (nextNode == nullptr) {
                    // Shouldn’t happen often if head != tail, but check
                    return -1;
                }
                int value = nextNode->value;

                // Try to swing head to next
                PtrVersion desiredHead = {nextNode, headSnap.version + 1};
                if (compareAndSwap128(head, headSnap, desiredHead)) {
                    delete headPtr; // reclaim old dummy node
                    return value;
                }
                // else someone beat us => retry
            }
        }
    }

    /**
     * Debug print. NOTE: Potentially dangerous in high concurrency, but fine for small tests.
     */
    void print() {
        PtrVersion h = head.load(std::memory_order_acquire);
        auto* cur = reinterpret_cast<Node*>(h.ptr);

        // skip dummy node
        PtrVersion nextSnap = cur->next.load(std::memory_order_acquire);
        cur = reinterpret_cast<Node*>(nextSnap.ptr);

        while (cur) {
            std::cout << cur->value << " ";
            nextSnap = cur->next.load(std::memory_order_acquire);
            cur = reinterpret_cast<Node*>(nextSnap.ptr);
        }
        std::cout << std::endl;
    }

private:
    /**
     * 16-byte CAS on a std::atomic<PtrVersion>.
     * Returns true if it swapped, false otherwise.
     */
    bool compareAndSwap128(std::atomic<PtrVersion> &atom,
                           PtrVersion &expected,
                           const PtrVersion &desired) {
        return __atomic_compare_exchange(
            &atom,          // object
            &expected,      // expected
            &desired,       // desired
            false,          // no spurious failures
            __ATOMIC_SEQ_CST,
            __ATOMIC_SEQ_CST
        );
    }
};

#endif // CONCURRENT_QUEUE_H
