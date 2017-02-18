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

#ifndef STATICLIB_CONTAINERS_BLOCKING_QUEUE_HPP
#define	STATICLIB_CONTAINERS_BLOCKING_QUEUE_HPP

#include <cstdint>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <type_traits>

namespace staticlib {
namespace containers {

/**
 * Optionally bounded FIFO queue implementation with synchronized access to all 
 * public methods. Supports multiple producers and multiple consumers.
 * Consumers will block on "take" from empty queue.
 */
template<typename T>
class blocking_queue { 
    mutable std::mutex mutex;
    std::condition_variable empty_cv;
    std::deque<T> delegate;
    size_t max_size;
    bool blocking = true;

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
    blocking_queue(size_t max_size = 0) : 
    max_size(max_size) { }

    /**
     * Emplace a value at the end of the queue
     * 
     * @param recordArgs constructor arguments for queue element
     * @return false if the queue was full, true otherwise
     */
    template<typename ...Args>
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
     * Emplace the values from specified range into
     * this queue
     * 
     * @param range source range
     * @return number of elements emplaced
     */
    template<typename Range,
            class = typename std::enable_if<!std::is_lvalue_reference<Range>::value>::type>
    size_t emplace_range(Range&& range) {
        std::lock_guard<std::mutex> guard{mutex};
        auto size = delegate.size();
        for (auto&& el : range) {
            if (0 == max_size || size < max_size) {
                delegate.emplace_back(std::move(el));
                size += 1;
            } else {
                break;
            }
        }
        return delegate.size() - size;
    }

    /**
     * Emplace the values from specified range into
     * this queue
     * 
     * @param range source range
     * @return number of elements emplaced
     */
    template<typename Range>
    size_t emplace_range(Range& range) {
        std::lock_guard<std::mutex> guard{mutex};
        auto origin_size = delegate.size();
        for (auto& el : range) {
            if (0 == max_size || delegate.size() < max_size) {
                delegate.emplace_back(el);
            } else {
                break;
            }
        }
        return delegate.size() - origin_size;
    }

    /**
     * Attempt to read the value at the front to the queue into a variable.
     * This method returns immediately.
     * 
     * @param record move (or copy) the value at the front of the queue to given variable
     * @return returns false if queue was empty, true otherwise
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
     * Consume all the contents of this queue into
     * specified functor
     * 
     * @param func functor to consume contents
     * @return number of elements consumed
     */
    template<typename Func>
    size_t consume(Func func) {
        std::lock_guard<std::mutex> guard{mutex};
        size_t count = 0;
        while(!delegate.empty()) {
            T record = std::move(delegate.front());
            delegate.pop_front();
            func(std::move(record));
            count += 1;
        }
        return count;
    }

    /**
     * Attempt to read the value at the front of the queue into a variable.
     * This method will wait on empty queue infinitely (by default), 
     * or up to specified amount of milliseconds
     * 
     * @param record move (or copy) the value at the front of the queue to given variable
     * @param timeout_millis max amount of milliseconds to wait on empty queue,
     *        negative value (supplied by default) will cause infinite wait
     * @return returns false if queue was empty after timeout, true otherwise
     */
    bool take(T& record, int32_t timeout_millis=-1) {
        std::unique_lock<std::mutex> lock{mutex};
        if (!delegate.empty()) {
            record = std::move(delegate.front());
            delegate.pop_front();
            return true;
        } else {
            auto predicate = [this] {
                return !this->blocking || !this->delegate.empty();
            };
            if (timeout_millis >= 0) {
                empty_cv.wait_for(lock, std::chrono::milliseconds{timeout_millis}, predicate);
            } else {
                empty_cv.wait(lock, predicate);
            }
            if (!delegate.empty()) {
                record = std::move(delegate.front());
                delegate.pop_front();
                return true;
            } else {
                return false;
            }
        }
    }
    
    /**
     * Unblocks the queue allowing consumers to
     * exit 'take' calls. Queue cannot be used
     * for waiting on it after this call.
     */
    void unblock() {
        std::lock_guard<std::mutex> guard{mutex};
        this->blocking = false;
        if (delegate.empty()) {
            empty_cv.notify_all();
        }
    }
    
    /**
     * Checks whether this queue was unblocked
     * 
     * @return whether this queue was unblocked
     */
    bool is_blocking() {
        std::lock_guard<std::mutex> guard{mutex};
        return blocking;
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


#endif	/* STATICLIB_CONTAINERS_BLOCKING_QUEUE_HPP */

