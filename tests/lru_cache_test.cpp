#include "miniredis/cache/lru_cache.hpp"

#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace miniredis::cache {
namespace {

TEST(LRUCacheTest, ReturnsNulloptForMissingKey) {
    LRUCache<std::string, std::string> cache(2);
    EXPECT_EQ(cache.get("missing"), std::nullopt);
}

TEST(LRUCacheTest, EvictsLeastRecentlyInsertedWhenFull) {
    LRUCache<int, int> cache(2);
    cache.put(1, 1);
    cache.put(2, 2);
    cache.put(3, 3);

    EXPECT_EQ(cache.get(1), std::nullopt);
    EXPECT_EQ(cache.get(2), 2);
    EXPECT_EQ(cache.get(3), 3);
    EXPECT_EQ(cache.size(), 2u);
}

TEST(LRUCacheTest, GetMarksEntryAsRecentlyUsed) {
    LRUCache<int, int> cache(2);
    cache.put(1, 1);
    cache.put(2, 2);
    cache.get(1);
    cache.put(3, 3);

    EXPECT_EQ(cache.get(1), 1);
    EXPECT_EQ(cache.get(2), std::nullopt);
}

TEST(LRUCacheTest, OverwritingKeyUpdatesValueAndRecencyWithoutEvicting) {
    LRUCache<int, std::string> cache(2);
    cache.put(1, "a");
    cache.put(2, "b");
    cache.put(1, "c");
    EXPECT_EQ(cache.size(), 2u);

    cache.put(3, "d");
    EXPECT_EQ(cache.get(1), "c");
    EXPECT_EQ(cache.get(2), std::nullopt);
}

TEST(LRUCacheTest, CapacityOneKeepsOnlyLatest) {
    LRUCache<int, int> cache(1);
    cache.put(1, 1);
    cache.put(2, 2);
    EXPECT_EQ(cache.get(1), std::nullopt);
    EXPECT_EQ(cache.get(2), 2);
}

TEST(LRUCacheTest, MixedSequence) {
    LRUCache<int, int> cache(3);
    cache.put(1, 1);
    cache.put(2, 2);
    EXPECT_EQ(cache.get(1), 1);
    cache.put(3, 3);
    EXPECT_EQ(cache.get(2), 2);
    cache.put(4, 4);
    EXPECT_EQ(cache.get(1), std::nullopt);
    EXPECT_EQ(cache.get(3), 3);
    EXPECT_EQ(cache.get(4), 4);
}

TEST(LRUCacheTest, RejectsNonPositiveCapacity) {
    using Cache = LRUCache<int, int>;
    EXPECT_THROW(Cache(0), std::invalid_argument);
    EXPECT_THROW(Cache(-1), std::invalid_argument);
}

TEST(LRUCacheTest, PutReturnsEvictedEntry) {
    LRUCache<int, std::string> cache(1);
    EXPECT_EQ(cache.put(1, "a"), std::nullopt);
    EXPECT_EQ(cache.put(1, "b"), std::nullopt);
    auto evicted = cache.put(2, "c");
    ASSERT_TRUE(evicted.has_value());
    EXPECT_EQ(evicted->first, 1);
    EXPECT_EQ(evicted->second, "b");
}

TEST(LRUCacheTest, EraseRemovesEntry) {
    LRUCache<int, int> cache(2);
    cache.put(1, 1);
    EXPECT_TRUE(cache.erase(1));
    EXPECT_FALSE(cache.erase(1));
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_EQ(cache.get(1), std::nullopt);

    cache.put(2, 2);
    cache.put(3, 3);
    cache.put(4, 4);
    EXPECT_EQ(cache.get(2), std::nullopt);
    EXPECT_EQ(cache.get(3), 3);
}

TEST(LRUCacheTest, FindAllowsInPlaceUpdateAndTouches) {
    LRUCache<int, int> cache(2);
    cache.put(1, 1);
    cache.put(2, 2);
    *cache.find(1) = 10;
    cache.put(3, 3);
    EXPECT_EQ(cache.get(1), 10);
    EXPECT_EQ(cache.get(2), std::nullopt);
    EXPECT_EQ(cache.find(42), nullptr);
}

TEST(LRUCacheTest, PeekDoesNotTouch) {
    LRUCache<int, int> cache(2);
    cache.put(1, 1);
    cache.put(2, 2);
    EXPECT_EQ(*cache.peek(1), 1);
    cache.put(3, 3);
    EXPECT_EQ(cache.peek(1), nullptr);
}

TEST(LRUCacheTest, ReportsCapacity) {
    LRUCache<int, int> cache(5);
    EXPECT_EQ(cache.capacity(), 5u);
}

}  // namespace
}  // namespace miniredis::cache
