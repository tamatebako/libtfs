/**
 * @file lru_cache.h
 * @brief Thread-safe LRU cache utility
 *
 * Provides a simple, efficient LRU (Least Recently Used) cache
 * that can be used for caching path lookups and other operations.
 *
 * Copyright (c) 2024-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 */

#pragma once

#include <list>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <functional>

namespace tebako {
namespace fs {

/**
 * @brief Thread-safe LRU cache
 *
 * A simple LRU cache implementation that evicts the least recently used
 * entries when the cache reaches its maximum size.
 *
 * @tparam Key The key type
 * @tparam Value The value type
 *
 * @example
 * @code
 * LRUCache<std::string, uint32_t> cache(1024);
 *
 * cache.put("/path/to/file", 42);
 * auto value = cache.get("/path/to/file");
 * if (value) {
 *     std::cout << "Found: " << *value << std::endl;
 * }
 * @endcode
 */
template<typename Key, typename Value>
class LRUCache {
public:
    using key_type = Key;
    using value_type = Value;
    using size_type = size_t;

private:
    // List stores (key, value) pairs with most recent at front
    using ListType = std::list<std::pair<Key, Value>>;
    using MapType = std::unordered_map<Key, typename ListType::iterator>;

    size_type max_size_;
    ListType list_;  // Most recent at front, least recent at back
    MapType map_;
    mutable std::mutex mutex_;

    // Statistics
    mutable size_type hits_ = 0;
    mutable size_type misses_ = 0;

public:
    /**
     * @brief Construct an LRU cache with specified maximum size
     * @param max_size Maximum number of entries (default: 1024)
     */
    explicit LRUCache(size_type max_size = 1024)
        : max_size_(max_size > 0 ? max_size : 1) {}

    /**
     * @brief Get a value from the cache
     * @param key The key to look up
     * @return The value if found, nullopt otherwise
     *
     * If found, the entry is moved to the front (most recently used).
     */
    std::optional<Value> get(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = map_.find(key);
        if (it == map_.end()) {
            ++misses_;
            return std::nullopt;
        }

        // Move to front (most recently used)
        list_.splice(list_.begin(), list_, it->second);
        ++hits_;
        return it->second->second;
    }

    /**
     * @brief Put a value into the cache
     * @param key The key
     * @param value The value
     *
     * If the key already exists, the value is updated and moved to front.
     * If the cache is full, the least recently used entry is evicted.
     */
    void put(const Key& key, Value value) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = map_.find(key);
        if (it != map_.end()) {
            // Update existing entry
            it->second->second = std::move(value);
            list_.splice(list_.begin(), list_, it->second);
            return;
        }

        // Add new entry
        if (list_.size() >= max_size_) {
            // Evict least recently used (at back)
            auto last = list_.back();
            map_.erase(last.first);
            list_.pop_back();
        }

        list_.emplace_front(key, std::move(value));
        map_[key] = list_.begin();
    }

    /**
     * @brief Check if a key exists in the cache
     * @param key The key to check
     * @return true if the key exists
     *
     * Note: This does NOT update the access order.
     */
    bool contains(const Key& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return map_.find(key) != map_.end();
    }

    /**
     * @brief Remove a key from the cache
     * @param key The key to remove
     * @return true if the key was found and removed
     */
    bool erase(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = map_.find(key);
        if (it == map_.end()) {
            return false;
        }

        list_.erase(it->second);
        map_.erase(it);
        return true;
    }

    /**
     * @brief Clear all entries from the cache
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        list_.clear();
        map_.clear();
        hits_ = 0;
        misses_ = 0;
    }

    /**
     * @brief Get the number of entries in the cache
     */
    size_type size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return list_.size();
    }

    /**
     * @brief Check if the cache is empty
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return list_.empty();
    }

    /**
     * @brief Get the maximum size of the cache
     */
    size_type max_size() const {
        return max_size_;
    }

    /**
     * @brief Set the maximum size of the cache
     * @param max_size The new maximum size
     *
     * If the current size exceeds the new max, entries are evicted.
     */
    void set_max_size(size_type max_size) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_size_ = max_size > 0 ? max_size : 1;

        // Evict entries if necessary
        while (list_.size() > max_size_) {
            auto last = list_.back();
            map_.erase(last.first);
            list_.pop_back();
        }
    }

    /**
     * @brief Get the cache hit ratio
     * @return Hit ratio as a value between 0.0 and 1.0
     */
    double hit_ratio() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_type total = hits_ + misses_;
        if (total == 0) {
            return 0.0;
        }
        return static_cast<double>(hits_) / static_cast<double>(total);
    }

    /**
     * @brief Get cache statistics
     */
    struct Stats {
        size_type size;
        size_type max_size;
        size_type hits;
        size_type misses;
        double hit_ratio;
    };

    /**
     * @brief Get all cache statistics
     */
    Stats stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_type total = hits_ + misses_;
        double ratio = total > 0 ? static_cast<double>(hits_) / static_cast<double>(total) : 0.0;

        return Stats{
            list_.size(),
            max_size_,
            hits_,
            misses_,
            ratio
        };
    }

    /**
     * @brief Reset statistics (does not clear cache entries)
     */
    void reset_stats() {
        std::lock_guard<std::mutex> lock(mutex_);
        hits_ = 0;
        misses_ = 0;
    }

    /**
     * @brief Get or compute a value
     * @param key The key to look up
     * @param compute Function to compute the value if not found
     * @return The cached or computed value
     *
     * This is useful for avoiding the "check-then-compute" race condition.
     */
    template<typename ComputeFunc>
    Value get_or_compute(const Key& key, ComputeFunc&& compute) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = map_.find(key);
        if (it != map_.end()) {
            list_.splice(list_.begin(), list_, it->second);
            ++hits_;
            return it->second->second;
        }

        ++misses_;

        // Compute the value
        Value value = std::forward<ComputeFunc>(compute)();

        // Add to cache
        if (list_.size() >= max_size_) {
            auto last = list_.back();
            map_.erase(last.first);
            list_.pop_back();
        }

        list_.emplace_front(key, value);
        map_[key] = list_.begin();

        return value;
    }
};

} // namespace fs
} // namespace tebako
