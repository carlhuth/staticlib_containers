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

#include "staticlib/containers/blocking_queue.hpp"

#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <sstream>

#include "staticlib/config/assert.hpp"

namespace sc = staticlib::containers;

const uint32_t ELEMENTS_COUNT = 1 << 10;

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
            MyMovableStr el{""};
            bool success = queue.take(el);
            slassert(success);
            slassert(el.get_val() == data[i]);
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
            MyMovableStr el{""};
            bool success = queue.take(el);
            slassert(success);
            slassert(42 == el.get_val().size());
        }
    });
    producer.join();
    consumer.join();
}

void test_multi() {
    sc::blocking_queue<MyMovableStr> queue{};
    auto take = [&](size_t count) {
        for (size_t i = 0; i < count; i++) {
            MyMovableStr el{""};
            bool success = queue.take(el);
            slassert(success);
            slassert(42 == el.get_val().size());
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
            slassert(success);
            slassert(el.get_val() == data[i]);
        }
        MyMovableStr el_fail{""};
        bool success = queue.poll(el_fail);
        slassert(!success);
    });
    consumer.join();
}

void test_take_wait() {
    sc::blocking_queue<MyMovableStr> queue{};
    std::thread producer([&queue] {
        std::this_thread::sleep_for(std::chrono::milliseconds{200});
        queue.emplace("aaa");
        std::this_thread::sleep_for(std::chrono::milliseconds{200});
        queue.emplace("bbb");
    });
    std::thread consumer([&queue] {
        // not yet available
        MyMovableStr el1{""};
        bool success1 = queue.take(el1, 100);
        slassert(!success1);
        slassert("" == el1.get_val());
        // first received
        MyMovableStr el2{""};
        bool success2 = queue.take(el2, 150);
        slassert(success2);
        slassert("aaa" == el2.get_val());
        // wait for next
        std::this_thread::sleep_for(std::chrono::milliseconds{200});
        // should be already there
        MyMovableStr el3{""};
        bool success3 = queue.take(el3, 10);
        slassert(success3);
        slassert("bbb" == el3.get_val());
    });

    producer.join();
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
    slassert(!emplaced);
    std::thread consumer([&] {
        for (size_t i = 0; i < ELEMENTS_COUNT; i++) {
            MyMovableStr el{""};
            bool success = queue.take(el);
            slassert(success);
            slassert(el.get_val() == data[i]);
        }
        auto ptr = queue.front_ptr();
        slassert(nullptr == ptr);
    });
    consumer.join();
}

void test_unblock() {
    sc::blocking_queue<MyMovableStr> queue{};
    std::thread consumer([&] {
        MyMovableStr el{""};
        bool success = queue.poll(el);
        slassert(!success);
        slassert(el.get_val() == "");
    });
    // ensure lock
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    queue.unblock();
    consumer.join();
}

void test_integral() {
    sc::blocking_queue<int> queue{};
    int a = 42;
    int b = 43;
    int& b_ref = b;
    queue.emplace(41);
    queue.emplace(a);
    queue.emplace(b_ref);
    slassert(3 == queue.size());
    int taken;
    slassert(queue.poll(taken));
    slassert(41 == taken);
    slassert(queue.poll(taken));
    slassert(42 == taken);
    slassert(queue.poll(taken));
    slassert(43 == taken);
    slassert(!queue.poll(taken));
}

int main() {
    try {
        test_take();
        test_intermittent();
        test_multi();
        test_poll();
        test_take_wait();
        test_threshold();
        test_unblock();
        test_integral();
    } catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }
    return 0;
}

