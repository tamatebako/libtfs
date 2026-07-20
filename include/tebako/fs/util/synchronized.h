/**
 *
 * Copyright (c) 2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 * This file is a part of the Tebako project. (libdwarfs-wr)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once

#include <shared_mutex>
#include <memory>
#include <mutex>
#include <utility>

namespace tebako {

/**
 * Thread-safe synchronized wrapper for data.
 * Provides reader-writer lock semantics using std::shared_mutex.
 *
 * This is a replacement for folly::Synchronized<T> that uses only
 * standard C++17 features.
 */
template <typename T>
class Synchronized {
 private:
  T data_;
  mutable std::shared_mutex mutex_;

 public:
  /**
   * Read lock holder (const access)
   * Allows multiple concurrent readers
   */
  class ConstLockedPtr {
   private:
    std::shared_lock<std::shared_mutex> lock_;
    const T* ptr_;

   public:
    ConstLockedPtr(std::shared_mutex& m, const T* p) : lock_(m), ptr_(p) {}

    const T* operator->() const { return ptr_; }
    const T& operator*() const { return *ptr_; }
  };

  /**
   * Write lock holder (mutable access)
   * Provides exclusive access for writing
   */
  class LockedPtr {
   private:
    std::unique_lock<std::shared_mutex> lock_;
    T* ptr_;

   public:
    LockedPtr(std::shared_mutex& m, T* p) : lock_(m), ptr_(p) {}

    T* operator->() { return ptr_; }
    T& operator*() { return *ptr_; }
  };

  // Constructors
  Synchronized() : data_() {}
  explicit Synchronized(T&& val) : data_(std::move(val)) {}
  explicit Synchronized(const T& val) : data_(val) {}

  // Deleted copy operations (not thread-safe to copy)
  Synchronized(const Synchronized&) = delete;
  Synchronized& operator=(const Synchronized&) = delete;

  /**
   * Acquire write lock for exclusive access
   * @return LockedPtr RAII wrapper that holds the lock
   */
  LockedPtr wlock() { return LockedPtr(mutex_, &data_); }

  /**
   * Acquire read lock for shared access
   * @return ConstLockedPtr RAII wrapper that holds the lock
   */
  ConstLockedPtr rlock() const { return ConstLockedPtr(mutex_, &data_); }

  /**
   * Atomic exchange operation
   * Swaps the current value with a new value and returns the old value
   * @param new_val New value to set
   * @return Old value
   */
  T exchange(T&& new_val)
  {
    auto lock = wlock();
    T old = std::move(*lock);
    *lock = std::move(new_val);
    return old;
  }
};

}  // namespace tebako