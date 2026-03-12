/**
 * @file test_lru_cache.cpp
 * @brief Unit tests for LRUCache
 */

#include <gtest/gtest.h>
#include <tebako/fs/cache/lru_cache.h>
#include <thread>
#include <vector>
#include <atomic>

using namespace tebako::fs;

// ============================================================================
// Basic Operations Tests
// ============================================================================

TEST(LRUCacheTest, EmptyCache)
{
  LRUCache<std::string, int> cache(10);
  EXPECT_TRUE(cache.empty());
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_FALSE(cache.get("key").has_value());
}

TEST(LRUCacheTest, PutAndGet)
{
  LRUCache<std::string, int> cache(10);

  cache.put("key1", 42);
  EXPECT_FALSE(cache.empty());
  EXPECT_EQ(cache.size(), 1u);

  auto value = cache.get("key1");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, 42);
}

TEST(LRUCacheTest, PutUpdatesExisting)
{
  LRUCache<std::string, int> cache(10);

  cache.put("key1", 42);
  cache.put("key1", 100);  // Update

  auto value = cache.get("key1");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, 100);
  EXPECT_EQ(cache.size(), 1u);
}

TEST(LRUCacheTest, Contains)
{
  LRUCache<std::string, int> cache(10);

  EXPECT_FALSE(cache.contains("key1"));
  cache.put("key1", 42);
  EXPECT_TRUE(cache.contains("key1"));
}

TEST(LRUCacheTest, Erase)
{
  LRUCache<std::string, int> cache(10);

  cache.put("key1", 42);
  EXPECT_TRUE(cache.contains("key1"));

  EXPECT_TRUE(cache.erase("key1"));
  EXPECT_FALSE(cache.contains("key1"));
  EXPECT_EQ(cache.size(), 0u);

  EXPECT_FALSE(cache.erase("nonexistent"));
}

TEST(LRUCacheTest, Clear)
{
  LRUCache<std::string, int> cache(10);

  cache.put("key1", 1);
  cache.put("key2", 2);
  cache.put("key3", 3);

  EXPECT_EQ(cache.size(), 3u);
  cache.clear();
  EXPECT_TRUE(cache.empty());
  EXPECT_EQ(cache.size(), 0u);
}

// ============================================================================
// LRU Eviction Tests
// ============================================================================

TEST(LRUCacheTest, EvictsLRU)
{
  LRUCache<std::string, int> cache(3);

  cache.put("a", 1);
  cache.put("b", 2);
  cache.put("c", 3);
  EXPECT_EQ(cache.size(), 3u);

  // Add fourth item - should evict "a"
  cache.put("d", 4);
  EXPECT_EQ(cache.size(), 3u);

  EXPECT_FALSE(cache.get("a").has_value());  // Evicted
  EXPECT_TRUE(cache.get("b").has_value());
  EXPECT_TRUE(cache.get("c").has_value());
  EXPECT_TRUE(cache.get("d").has_value());
}

TEST(LRUCacheTest, AccessUpdatesLRU)
{
  LRUCache<std::string, int> cache(3);

  cache.put("a", 1);
  cache.put("b", 2);
  cache.put("c", 3);

  // Access "a" - makes it most recently used
  cache.get("a");

  // Add fourth item - should evict "b" (now least recently used)
  cache.put("d", 4);

  EXPECT_TRUE(cache.get("a").has_value());   // Still there
  EXPECT_FALSE(cache.get("b").has_value());  // Evicted
  EXPECT_TRUE(cache.get("c").has_value());
  EXPECT_TRUE(cache.get("d").has_value());
}

TEST(LRUCacheTest, UpdateMovesToFront)
{
  LRUCache<std::string, int> cache(3);

  cache.put("a", 1);
  cache.put("b", 2);
  cache.put("c", 3);

  // Update "a" - makes it most recently used
  cache.put("a", 10);

  // Add fourth item - should evict "b"
  cache.put("d", 4);

  EXPECT_TRUE(cache.get("a").has_value());
  EXPECT_FALSE(cache.get("b").has_value());  // Evicted
  EXPECT_TRUE(cache.get("c").has_value());
  EXPECT_TRUE(cache.get("d").has_value());
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST(LRUCacheTest, HitMissStats)
{
  LRUCache<std::string, int> cache(10);

  cache.put("key1", 42);

  // Hit
  auto v1 = cache.get("key1");
  EXPECT_TRUE(v1.has_value());

  // Miss
  auto v2 = cache.get("nonexistent");
  EXPECT_FALSE(v2.has_value());

  auto stats = cache.stats();
  EXPECT_EQ(stats.hits, 1u);
  EXPECT_EQ(stats.misses, 1u);
  EXPECT_DOUBLE_EQ(stats.hit_ratio, 0.5);
}

TEST(LRUCacheTest, ResetStats)
{
  LRUCache<std::string, int> cache(10);

  cache.put("key1", 42);
  cache.get("key1");
  cache.get("nonexistent");

  cache.reset_stats();

  auto stats = cache.stats();
  EXPECT_EQ(stats.hits, 0u);
  EXPECT_EQ(stats.misses, 0u);
}

// ============================================================================
// Max Size Tests
// ============================================================================

TEST(LRUCacheTest, SetMaxSizeShrinks)
{
  LRUCache<std::string, int> cache(10);

  cache.put("a", 1);
  cache.put("b", 2);
  cache.put("c", 3);
  cache.put("d", 4);
  cache.put("e", 5);

  EXPECT_EQ(cache.size(), 5u);

  // Shrink cache - should evict oldest entries
  cache.set_max_size(2);
  EXPECT_EQ(cache.size(), 2u);
  EXPECT_EQ(cache.max_size(), 2u);

  // "d" and "e" should remain (most recently added)
  EXPECT_FALSE(cache.get("a").has_value());
  EXPECT_FALSE(cache.get("b").has_value());
  EXPECT_FALSE(cache.get("c").has_value());
  EXPECT_TRUE(cache.get("d").has_value());
  EXPECT_TRUE(cache.get("e").has_value());
}

TEST(LRUCacheTest, MinSizeIsOne)
{
  LRUCache<std::string, int> cache(0);  // Should become 1
  EXPECT_EQ(cache.max_size(), 1u);

  cache.set_max_size(0);  // Should stay 1
  EXPECT_EQ(cache.max_size(), 1u);
}

// ============================================================================
// Get Or Compute Tests
// ============================================================================

TEST(LRUCacheTest, GetOrCompute_Cached)
{
  LRUCache<std::string, int> cache(10);
  cache.put("key1", 42);

  int call_count = 0;
  auto value = cache.get_or_compute("key1", [&]() {
    ++call_count;
    return 100;
  });

  EXPECT_EQ(value, 42);      // Returned cached value
  EXPECT_EQ(call_count, 0);  // Never called
}

TEST(LRUCacheTest, GetOrCompute_Compute)
{
  LRUCache<std::string, int> cache(10);

  int call_count = 0;
  auto value = cache.get_or_compute("key1", [&]() {
    ++call_count;
    return 100;
  });

  EXPECT_EQ(value, 100);
  EXPECT_EQ(call_count, 1);

  // Second call should use cached value
  auto value2 = cache.get_or_compute("key1", [&]() {
    ++call_count;
    return 200;
  });

  EXPECT_EQ(value2, 100);
  EXPECT_EQ(call_count, 1);  // Not called again
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST(LRUCacheTest, ThreadSafe_ParallelPut)
{
  LRUCache<int, int> cache(1000);
  const int num_threads = 10;
  const int items_per_thread = 100;

  std::vector<std::thread> threads;
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&cache, t, items_per_thread]() {
      for (int i = 0; i < items_per_thread; ++i) {
        int key = t * items_per_thread + i;
        cache.put(key, key * 2);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // All items should be in cache (if not evicted due to size)
  // At least the most recent ones
  EXPECT_GT(cache.size(), 0u);
}

TEST(LRUCacheTest, ThreadSafe_ParallelGetPut)
{
  LRUCache<int, int> cache(100);
  std::atomic<int> total_hits{0};
  std::atomic<int> total_misses{0};

  // Pre-populate
  for (int i = 0; i < 50; ++i) {
    cache.put(i, i * 2);
  }

  std::vector<std::thread> threads;
  for (int t = 0; t < 10; ++t) {
    threads.emplace_back([&cache, &total_hits, &total_misses]() {
      for (int i = 0; i < 100; ++i) {
        auto value = cache.get(i);
        if (value) {
          ++total_hits;
        }
        else {
          ++total_misses;
          cache.put(i, i * 2);
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // Should have some hits and misses
  EXPECT_GT(total_hits.load(), 0);
  EXPECT_GT(total_misses.load(), 0);
}

// ============================================================================
// Different Key/Value Types
// ============================================================================

TEST(LRUCacheTest, IntKeyIntValue)
{
  LRUCache<int, int> cache(10);
  cache.put(42, 100);
  auto value = cache.get(42);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, 100);
}

TEST(LRUCacheTest, StringKeyStringValue)
{
  LRUCache<std::string, std::string> cache(10);
  cache.put("key", "value");
  auto value = cache.get("key");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, "value");
}
