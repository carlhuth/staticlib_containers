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
 * File:   blocking_queue.hpp
 * Author: alex
 *
 * Created on July 2, 2015, 3:20 PM
 */

#ifndef STATICLIB_BLOCKING_QUEUE_HPP
#define	STATICLIB_BLOCKING_QUEUE_HPP

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <cstdint>

namespace staticlib {
namespace containers {

/**
 * Optionally bounded FIFO queue implementation with synchronised access to all 
 * public methods. Supports multiple producers and multiple consumers.
 * Consumers will block on "take" from empty queue.
 */
template<typename T>
class blocking_queue { 
    std::deque<T> delegate;
    std::mutex mutex;
    uint32_t max_size;
    std::condition_variable empty_cv;

    /**
     * Deleted copy constructor
     * 
     * @param other instance
     */
    blocking_queue(const blocking_queue&) = delete;

    /**
     * Deleted copy assignment operator
     * 
     * @param other instance
     * @return reference to self
     */
    blocking_queue& operator=(const blocking_queue&) = delete;
    
public:
    /**
     * Type of elements
     */
    typedef T value_type;

    /**
     * Constructor, optional bound size can be specified,
     * unbounded by default
     * 
     * @param max_size queue size bound
     */
    explicit blocking_queue(uint32_t max_size = 0) : 
    max_size(max_size) { }

    /**
     * Emplace a value at the end of the queue
     * 
     * @param recordArgs constructor arguments for queue element
     * @return false if the queue was full, true otherwise
     */
    template<class ...Args>
    bool emplace(Args&&... record_args) {
        std::lock_guard<std::mutex> guard{mutex};
        auto size = delegate.size();
        if (0 == max_size || size < max_size) {
            delegate.emplace_back(std::forward<Args>(record_args)...);
            if (0 == size) {
                // notify_one causes deadlocks here
                empty_cv.notify_all();
            }
            return true;
        } else {
            return false;
        }
    }

    /**
     * Remove and return element from the front of the queue,
     * will block until queue became non-empty
     * 
     * @param record move (or copy) the value at the front of the queue to given variable
     * @return  returns value from queue front
     */
    T take() {
        std::unique_lock<std::mutex> lock{mutex};
        if (!delegate.empty()) {
            T res = std::move(delegate.front());
            delegate.pop_front();
            return res;
        } else {
            empty_cv.wait(lock, [this]{return !this->delegate.empty();});
            assert(!delegate.empty());
            T res = std::move(delegate.front());
            delegate.pop_front();
            return res;
        }
    }

    /**
     * Attempt to read the value at the front to the queue into a variable
     * 
     * @param record move (or copy) the value at the front of the queue to given variable
     * @return  returns false if queue was empty, true otherwise
     */
    bool poll(T& record) {
        std::lock_guard<std::mutex> guard{mutex};
        if (!delegate.empty()) {
            record = std::move(delegate.front());
            delegate.pop_front();
            return true;
        } else {
            return false;
        }
    }

    /**
     * Retrieve a pointer to the item at the front of the queue
     * 
     * @return a pointer to the item, nullptr if it is empty
     */
    T* front_ptr() {
        std::lock_guard<std::mutex> guard{mutex};
        if (!delegate.empty()) {
            return &delegate.front();
        } else {
            return nullptr;
        }
    }

    /**
     * Check if the queue is empty
     * 
     * @return whether queue is empty
     */
    bool is_empty() const {
        std::lock_guard<std::mutex> guard{mutex};
        return delegate.empty();
    }

    /**
     * Check if the queue is full, always false for unbounded queue
     * 
     * @return whether queue is full
     */
    bool is_full() const {
        std::lock_guard<std::mutex> guard{mutex};
        if (0 == max_size) return false;
        return delegate.size() >= max_size;
    }

    /**
     * Returns the number of entries in the queue
     * 
     * @return number of entries in the queue
     */
    size_t size() const {
        std::lock_guard<std::mutex> guard{mutex};
        return delegate.size();
    }
};

}
} // namespace


#endif	/* STATICLIB_BLOCKING_QUEUE_HPP */

