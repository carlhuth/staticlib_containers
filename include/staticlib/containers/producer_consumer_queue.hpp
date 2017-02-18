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
 * Copyright 2015 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// @author Bo Hu (bhu@fb.com)
// @author Jordan DeLong (delong.j@fb.com)

// source: https://github.com/facebook/folly/blob/b75ef0a0af48766298ebcc946dd31fe0da5161e3/folly/ProducerConsumerQueue.h

#ifndef STATICLIB_CONTAINERS_PRODUCER_CONSUMER_QUEUE_HPP
#define STATICLIB_CONTAINERS_PRODUCER_CONSUMER_QUEUE_HPP

#include <cstdlib>
#include <atomic>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace staticlib {
namespace containers {

/**
 * ProducerConsumerQueue is a one producer and one consumer queue
 * without locks. 
 * See docs: https://github.com/facebook/folly/blob/master/folly/docs/ProducerConsumerQueue.md
 * Note, 'popFront' method was removed as it didn't work properly with MSVC.
 */
template<typename T>
class producer_consumer_queue {        
    const uint32_t size_;
    T * const records_;

    std::atomic<unsigned int> readIndex_;
    std::atomic<unsigned int> writeIndex_;
    
    /**
     * Deleted copy constructor
     * 
     * @param other instance
     */
    producer_consumer_queue(const producer_consumer_queue&) = delete;
    
    /**
     * Deleted copy assignment operator
     * 
     * @param other instance
     * @return reference to self
     */
    producer_consumer_queue& operator=(const producer_consumer_queue&) = delete;

public:
    /**
     * Type of elements
     */
    typedef T value_type;
    
    /**
     * Constructor,
     * 
     * @param size queue size, must be >= 1
     */
    explicit producer_consumer_queue(uint32_t size) : 
    size_(size + 1), 
    records_(static_cast<T*> (std::malloc(sizeof (T) * (size + 1)))), 
    readIndex_(0), 
    writeIndex_(0) {
//        assert(size >= 2);
        if (!records_) {
            throw std::bad_alloc();
        }
    }

    /**
     * Destructor
     */
    ~producer_consumer_queue() {
        // We need to destruct anything that may still exist in our queue.
        // (No real synchronization needed at destructor time: only one
        // thread can be doing this.)
        
        // check disabled, still safe, may be slower
//        if (!boost::has_trivial_destructor<T>::value) {
            size_t read = readIndex_;
            size_t end = writeIndex_;
            while (read != end) {
                records_[read].~T();
                if (++read == size_) {
                    read = 0;
                }
            }
//        }

        std::free(records_);
    }

    /**
     * Emplace a value at the end of the queue
     * 
     * @param recordArgs constructor arguments for queue element
     * @return false if the queue was full, true otherwise
     */
    template<class ...Args>
    bool emplace(Args&&... record_args) {
        auto const currentWrite = writeIndex_.load(std::memory_order_relaxed);
        auto nextRecord = currentWrite + 1;
        if (nextRecord == size_) {
            nextRecord = 0;
        }
        if (nextRecord != readIndex_.load(std::memory_order_acquire)) {
            new (&records_[currentWrite]) T(std::forward<Args>(record_args)...);
            writeIndex_.store(nextRecord, std::memory_order_release);
            return true;
        }

        // queue is full
        return false;
    }

    /**
     * Attempt to read the value at the front to the queue into a variable
     * 
     * @param record move (or copy) the value at the front of the queue to given variable
     * @return  returns false if queue was empty, true otherwise
     */
    bool poll(T& record) {
        auto const currentRead = readIndex_.load(std::memory_order_relaxed);
        if (currentRead == writeIndex_.load(std::memory_order_acquire)) {
            // queue is empty
            return false;
        }

        auto nextRecord = currentRead + 1;
        if (nextRecord == size_) {
            nextRecord = 0;
        }
        record = std::move(records_[currentRead]);
        records_[currentRead].~T();
        readIndex_.store(nextRecord, std::memory_order_release);
        return true;
    }

    /**
     * Retrieve a pointer to the item at the front of the queue
     * 
     * @return a pointer to the item, nullptr if it is empty
     */
    T* front_ptr() {
        auto const currentRead = readIndex_.load(std::memory_order_relaxed);
        if (currentRead == writeIndex_.load(std::memory_order_acquire)) {
            // queue is empty
            return nullptr;
        }
        return &records_[currentRead];
    }

    /**
     * Check if the queue is empty
     * 
     * @return whether queue is empty
     */
    bool is_empty() const {
        return readIndex_.load(std::memory_order_acquire) ==
                writeIndex_.load(std::memory_order_acquire);
    }

    /**
     * Check if the queue is full
     * 
     * @return whether queue is full
     */
    bool is_full() const {
        auto nextRecord = writeIndex_.load(std::memory_order_acquire) + 1;
        if (nextRecord == size_) {
            nextRecord = 0;
        }
        if (nextRecord != readIndex_.load(std::memory_order_acquire)) {
            return false;
        }
        // queue is full
        return true;
    }

    /**
     * Returns the number of entries in the queue.
     * If called by consumer, then true size may be more (because producer may
     * be adding items concurrently).
     * If called by producer, then true size may be less (because consumer may
     * be removing items concurrently).
     * It is undefined to call this from any other thread.
     * 
     * @return number of entries in the queue
     */
    size_t size_guess() const {
        int ret = writeIndex_.load(std::memory_order_acquire) -
                readIndex_.load(std::memory_order_acquire);
        if (ret < 0) {
            ret += size_;
        }
        return ret;
    }
    
    /**
     * Accessor for max queue size specified at creation
     * 
     * @return max queue size
     */
    size_t max_size() const {
        return size_ - 1;
    }
};

}
} //namespace

#endif /* STATICLIB_CONTAINERS_PRODUCER_CONSUMER_QUEUE_HPP */
