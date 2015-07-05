/*
 * Copyright 2015, alex at staticlibs.net
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* 
 * File:   ProducerConsumerQueue_test.cpp
 * Author: alex
 *
 * Created on June 28, 2015, 8:02 PM
 */

// source: https://github.com/facebook/folly/blob/b75ef0a0af48766298ebcc946dd31fe0da5161e3/folly/test/ProducerConsumerQueueTest.cpp

#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "staticlib/containers/producer_consumer_queue.hpp"

namespace { // anonymous

namespace sc = staticlib::containers;

template<class T> struct TestTraits {

    T limit() const {
        return 1 << 12;
    }

    T generate() const {
        return rand() % 26;
    }
};

template<> struct TestTraits<std::string> {

    unsigned int limit() const {
        return 1 << 12;
    }

    std::string generate() const {
        return std::string(12, '#');
    }
};

template<class QueueType, size_t Size>
struct PerfTest {
    typedef typename QueueType::value_type T;

    explicit PerfTest() : queue_(Size), done_(false) { }

    void operator()() {
        using namespace std::chrono;
        auto const startTime = system_clock::now();
        std::thread producer([this] { this->producer(); });
        std::thread consumer([this] { this->consumer(); });
        producer.join();
        done_ = true;
        consumer.join();
        auto duration = duration_cast<milliseconds>(system_clock::now() - startTime);
        std::cout << " done: " << duration.count() << "ms" << std::endl;
    }

    void producer() {
        // This is written differently than you might expect so that
        // it does not run afoul of -Wsign-compare, regardless of the
        // signedness of this loop's upper bound.
        for (auto i = traits_.limit(); i > 0; --i) {
            while (!queue_.emplace(traits_.generate())) {
            }
        }
    }

    void consumer() {
        while (!done_) {
            T data;
            queue_.poll(data);
        }
    }
    QueueType queue_;
    std::atomic<bool> done_;
    TestTraits<T> traits_;
};

template<class TestType> void doTest(const char* name) {
    std::cout << " testing: " << name << std::endl;
    std::unique_ptr<TestType> const t(new TestType());
    (*t)();
}

template<class T, bool Pop = false >
void perfTestType(const char* type) {
    const size_t size = 0xfffe;
    std::cout << "Type: " << type << std::endl;
    doTest<PerfTest<sc::producer_consumer_queue<T>, size> >("ProducerConsumerQueue");
}

template<class QueueType, size_t Size>
struct CorrectnessTest {
    typedef typename QueueType::value_type T;

    std::vector<T> testData_;
    QueueType queue_;
    TestTraits<T> traits_;
    std::atomic<bool> done_;

    explicit CorrectnessTest():
    queue_(Size),
    done_(false) {
        const size_t testSize = static_cast<size_t>(traits_.limit());
        testData_.reserve(testSize);
        for (size_t i = 0; i < testSize; ++i) {
            testData_.push_back(traits_.generate());
        }
    }

    void operator()() {
        std::thread producer([this] { this->producer(); });
        std::thread consumer([this] { this->consumer(); });
        producer.join();
        done_ = true;
        consumer.join();
    }

    void producer() {
        for (auto& data : testData_) {
            while (!queue_.emplace(data)) {
            }
        }
    }

    void consumer() {
        for (auto expect : testData_) {
        again:
            T data;
            if (!queue_.poll(data)) {
                if (done_) {
                    // Try one more read; unless there's a bug in the queue class
                    // there should still be more data sitting in the queue even
                    // though the producer thread exited.
                    if (!queue_.poll(data)) {
                        assert(false); // Finished too early ...
                        return;
                    }
                } else {
                    goto again;
                }
            }
            (void) expect;
            assert(data == expect);
        }
    }
};

template<class T>
void correctnessTestType(const std::string& type) {
    std::cout << "Type: " << type << std::endl;
    doTest<CorrectnessTest<sc::producer_consumer_queue<T>, 0xfffe> >("ProducerConsumerQueue");
}

struct DtorChecker {
    static unsigned int numInstances;

    DtorChecker() {
        ++numInstances;
    }

    DtorChecker(const DtorChecker&) {
        ++numInstances;
    }

    ~DtorChecker() {
        --numInstances;
    }
};

unsigned int DtorChecker::numInstances = 0;

void test_QueueCorrectness() {
    correctnessTestType<std::string>("string");
    correctnessTestType<int>("int");
    correctnessTestType<unsigned long long>("unsigned long long");
}

void test_PerfTest() {
    perfTestType<std::string>("string");
    perfTestType<int>("int");
    perfTestType<unsigned long long>("unsigned long long");
}

void test_Destructor() {
    // Test that orphaned elements in a ProducerConsumerQueue are
    // destroyed.
    {
        sc::producer_consumer_queue<DtorChecker> queue(1024);
        for (int i = 0; i < 10; ++i) {
            assert(queue.emplace(DtorChecker()));
        }
        assert(DtorChecker::numInstances == 10);
        {
            DtorChecker ignore;
            assert(queue.poll(ignore));
            assert(queue.poll(ignore));
        }
        assert(DtorChecker::numInstances == 8);
    }
    assert(DtorChecker::numInstances == 0);
    // Test the same thing in the case that the queue write pointer has
    // wrapped, but the read one hasn't.
    {
        sc::producer_consumer_queue<DtorChecker> queue(4);
        for (int i = 0; i < 3; ++i) {
            assert(queue.emplace(DtorChecker()));
        }
        assert(DtorChecker::numInstances == 3);
        {
            DtorChecker ignore;
            assert(queue.poll(ignore));
        }
        assert(DtorChecker::numInstances == 2);
        assert(queue.emplace(DtorChecker()));
        assert(DtorChecker::numInstances == 3);
    }
    assert(DtorChecker::numInstances == 0);
}

void test_EmptyFull() {
    sc::producer_consumer_queue<int> queue(3);
    assert(queue.is_empty());
    assert(!queue.is_full());
    assert(queue.emplace(1));
    assert(!queue.is_empty());
    assert(!queue.is_full());
    assert(queue.emplace(2));
    assert(!queue.is_empty());
    assert(queue.is_full()); // Tricky: full after 2 writes, not 3.
    assert(!queue.emplace(3));
    assert(queue.size_guess() == 2);
}

}

int main() {
    test_QueueCorrectness();
    test_PerfTest();
    test_Destructor();
    test_EmptyFull();
    
    return 0;
}

