#include "../../src/transcribe/concurrentQueue.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

void testPushAndPop() {
    ConcurrentQueue<int> queue;
    queue.push(42);
    assert(queue.size() == 1);
    int value = queue.waitAndPop();
    assert(value == 42);
    assert(queue.empty());
}

void testTryPop() {
    ConcurrentQueue<int> queue;
    assert(!queue.tryPop().has_value());
    queue.push(7);
    auto val = queue.tryPop();
    assert(val.has_value() && *val == 7);
    assert(queue.empty());
}

void testThreaded() {
    ConcurrentQueue<int> queue;
    std::thread producer([&]() {
        for (int i = 0; i < 10; ++i) queue.push(i);
    });
    std::thread consumer([&]() {
        for (int i = 0; i < 10; ++i) {
            int v = queue.waitAndPop();
            assert(v == i);
        }
    });
    producer.join();
    consumer.join();
    assert(queue.empty());
}

int main() {
    testPushAndPop();
    testTryPop();
    testThreaded();
    std::cout << "ConcurrentQueue tests passed.\n";
    return 0;
} 