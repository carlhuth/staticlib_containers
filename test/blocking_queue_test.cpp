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
 * File:   blocking_queue_test.cpp
 * Author: alex
 *
 * Created on July 2, 2015, 11:08 PM
 */

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <sstream>
#include <cstdint>
#include <cassert>

#include "staticlib/containers/blocking_queue.hpp"

namespace { // anonymous

namespace sc = staticlib::containers;

const size_t ELEMENTS_COUNT = 1 << 10;

template<typename T>
std::string to_string(T t) {
    std::stringstream ss{};
    ss << t;
    return ss.str();
}

class TestStringGenerator {
    uint32_t counter = 0;
    
public:
    std::string generate(uint32_t size) {
        return std::string(size, to_string(counter++)[0]);
    }
};

class MyMovableStr {
    std::string val;
public:
    MyMovableStr(std::string val) : val(val) { }

    const std::string& get_val() const {
        return val;
    }

    MyMovableStr(const MyMovableStr&) = delete;
    MyMovableStr& operator=(const MyMovableStr&) = delete;

    MyMovableStr(MyMovableStr&& other) :
    val(other.val) {
        other.val = "";
    };

    MyMovableStr& operator=(MyMovableStr&& other) {
        this->val = other.val;
        other.val = "";
        return *this;
    }
};

void test_take() {
    TestStringGenerator gen{};
    std::vector<std::string> data{};
    sc::blocking_queue<MyMovableStr> queue{};
    for (size_t i = 0; i < ELEMENTS_COUNT; i++) {
        std::string str = gen.generate(42);
        data.push_back(str);
        queue.emplace(std::move(str));
    }
    std::thread consumer([&]{
        for (size_t i = 0; i < ELEMENTS_COUNT; i++) {
            auto el = queue.take();
            assert(el.get_val() == data[i]);
        }
    });
    consumer.join();
}

void test_intermittent() {
    sc::blocking_queue<MyMovableStr> queue{};
    std::thread producer([&] {
        TestStringGenerator gen{};
        for (size_t i = 0; i < 10; i++) {
            std::string str = gen.generate(42);
            queue.emplace(std::move(str));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{200});
        for (size_t i = 10; i < 20; i++) {
            std::string str = gen.generate(42);
            queue.emplace(std::move(str));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{300});
        for (size_t i = 20; i < ELEMENTS_COUNT; i++) {
            std::string str = gen.generate(42);
            queue.emplace(std::move(str));
        }
    });
    std::thread consumer([&] {
        for (size_t i = 0; i < ELEMENTS_COUNT; i++) {
            auto el = queue.take();
            (void) el; assert(42 == el.get_val().size());
        }
    });
    producer.join();
    consumer.join();
}

void test_multi() {
    sc::blocking_queue<MyMovableStr> queue{};
    auto take = [&](size_t count) {
        for (size_t i = 0; i < count; i++) {
            auto el = queue.take();
            (void) el;
            assert(42 == el.get_val().size());
        }
    };
    auto put = [&](size_t count) {
        TestStringGenerator gen{};
        for (size_t i = 0; i < count; i++) {
            std::string str = gen.generate(42);
            queue.emplace(std::move(str));
        }
    };
    std::thread producer1(put, 100);
    std::thread producer2(put, 100);
    std::thread producer3(put, 100);
    std::thread consumer1(take, 50);
    std::thread consumer2(take, 50);
    std::thread consumer3(take, 50);
    std::thread consumer4(take, 50);
    std::thread consumer5(take, 50);
    std::thread consumer6(take, 50);
    producer1.join();
    producer2.join();
    producer3.join();
    consumer1.join();
    consumer2.join();
    consumer3.join();
    consumer4.join();
    consumer5.join();
    consumer6.join();
}

void test_poll() {
    TestStringGenerator gen{};
    std::vector<std::string> data{};
    sc::blocking_queue<MyMovableStr> queue{};
    for (size_t i = 0; i < ELEMENTS_COUNT; i++) {
        std::string str = gen.generate(42);
        data.push_back(str);
        queue.emplace(std::move(str));
    }
    std::thread consumer([&] {
        for (size_t i = 0; i < ELEMENTS_COUNT; i++) {
            MyMovableStr el{""};
            bool success = queue.poll(el);
            (void) success; assert(success);
            assert(el.get_val() == data[i]);
        }
        MyMovableStr el_fail{""};
        bool success = queue.poll(el_fail);
        (void) success; assert(!success);
    });
    consumer.join();

}

void test_threshold() {
    TestStringGenerator gen{};
    std::vector<std::string> data{};
    sc::blocking_queue<MyMovableStr> queue{ELEMENTS_COUNT};
    for (size_t i = 0; i < ELEMENTS_COUNT; i++) {
        std::string str = gen.generate(42);
        data.push_back(str);
        queue.emplace(std::move(str));
    }
    bool emplaced = queue.emplace("");
    (void) emplaced; assert(!emplaced);
    std::thread consumer([&] {
        for (size_t i = 0; i < ELEMENTS_COUNT; i++) {
            auto el = queue.take();
            assert(el.get_val() == data[i]);
        }
        auto ptr = queue.front_ptr();
        (void) ptr; assert(nullptr == ptr);
    });
    consumer.join();
}

} // namespace

int main() {
    test_take();
    test_intermittent();
    test_multi();
    test_poll();
    test_threshold();

    return 0;
}

