#include "miniredis/store/key_value_store.hpp"

#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "manual_clock.hpp"

namespace miniredis::store {
namespace {

using namespace std::chrono_literals;
using miniredis::testing::ManualClock;

class KeyValueStoreTest : public ::testing::Test {
protected:
    ManualClock clock;
    KeyValueStore store{100, clock};
};

TEST_F(KeyValueStoreTest, SetThenGet) {
    store.set("k", "v");
    EXPECT_EQ(store.get("k"), "v");
    EXPECT_EQ(store.get("missing"), std::nullopt);
}

TEST_F(KeyValueStoreTest, DelReportsWhetherKeyExisted) {
    store.set("k", "v");
    EXPECT_TRUE(store.del("k"));
    EXPECT_FALSE(store.del("k"));
    EXPECT_EQ(store.get("k"), std::nullopt);
}

TEST_F(KeyValueStoreTest, PttlDistinguishesMissingPersistentAndExpiring) {
    EXPECT_EQ(store.pttl("k"), KeyValueStore::kNoKey);
    store.set("k", "v");
    EXPECT_EQ(store.pttl("k"), KeyValueStore::kNoExpiry);
    store.set("t", "v", 1500ms);
    EXPECT_EQ(store.pttl("t"), 1500);
    clock.advance(500ms);
    EXPECT_EQ(store.pttl("t"), 1000);
}

TEST_F(KeyValueStoreTest, KeyExpiresExactlyAtDeadline) {
    store.set("k", "v", 1000ms);
    clock.advance(999ms);
    EXPECT_EQ(store.get("k"), "v");
    clock.advance(1ms);
    EXPECT_EQ(store.get("k"), std::nullopt);
    EXPECT_EQ(store.pttl("k"), KeyValueStore::kNoKey);
}

TEST_F(KeyValueStoreTest, ReadingAnExpiredKeyRemovesIt) {
    store.set("k", "v", 10ms);
    clock.advance(10ms);
    EXPECT_EQ(store.size(), 1u);
    EXPECT_EQ(store.get("k"), std::nullopt);
    EXPECT_EQ(store.size(), 0u);
}

TEST_F(KeyValueStoreTest, ExpireSetsDeadlineOnExistingKeyOnly) {
    EXPECT_FALSE(store.expire("missing", 1000ms));
    store.set("k", "v");
    EXPECT_TRUE(store.expire("k", 1000ms));
    EXPECT_EQ(store.pttl("k"), 1000);
    EXPECT_TRUE(store.expire("k", 5000ms));
    clock.advance(2000ms);
    EXPECT_EQ(store.get("k"), "v");
}

TEST_F(KeyValueStoreTest, NonPositiveExpireDeletes) {
    store.set("k", "v");
    EXPECT_TRUE(store.expire("k", 0ms));
    EXPECT_EQ(store.get("k"), std::nullopt);
    store.set("k", "v");
    EXPECT_TRUE(store.expire("k", -5ms));
    EXPECT_EQ(store.get("k"), std::nullopt);
}

TEST_F(KeyValueStoreTest, ExpireOnExpiredKeyReportsMissing) {
    store.set("k", "v", 10ms);
    clock.advance(20ms);
    EXPECT_FALSE(store.expire("k", 1000ms));
    EXPECT_FALSE(store.del("k"));
}

TEST_F(KeyValueStoreTest, SetClearsPreviousDeadline) {
    store.set("k", "v", 10ms);
    store.set("k", "v2");
    EXPECT_EQ(store.pttl("k"), KeyValueStore::kNoExpiry);
    clock.advance(20ms);
    EXPECT_EQ(store.get("k"), "v2");
    EXPECT_EQ(store.purgeExpired(100), 0u);
}

TEST_F(KeyValueStoreTest, RejectsNonPositiveSetTtl) {
    EXPECT_THROW(store.set("k", "v", 0ms), std::invalid_argument);
    EXPECT_EQ(store.get("k"), std::nullopt);
}

TEST_F(KeyValueStoreTest, PurgeRemovesOnlyExpiredKeysInBatches) {
    for (int i = 0; i < 10; ++i) {
        store.set("short" + std::to_string(i), "v", 10ms);
    }
    store.set("long", "v", 1000ms);
    store.set("forever", "v");
    clock.advance(10ms);

    EXPECT_EQ(store.purgeExpired(4), 4u);
    EXPECT_EQ(store.purgeExpired(4), 4u);
    EXPECT_EQ(store.purgeExpired(4), 2u);
    EXPECT_EQ(store.purgeExpired(4), 0u);
    EXPECT_EQ(store.size(), 2u);
    EXPECT_EQ(store.get("long"), "v");
    EXPECT_EQ(store.get("forever"), "v");
}

TEST(KeyValueStoreEvictionTest, LruEvictionAlsoDropsDeadline) {
    ManualClock clock;
    KeyValueStore store(2, clock);
    store.set("a", "1", 10ms);
    store.set("b", "2");
    store.set("c", "3");
    EXPECT_EQ(store.get("a"), std::nullopt);
    clock.advance(10ms);
    EXPECT_EQ(store.purgeExpired(100), 0u);
    EXPECT_EQ(store.size(), 2u);
}

TEST(KeyValueStoreEvictionTest, ReadsRefreshRecency) {
    ManualClock clock;
    KeyValueStore store(2, clock);
    store.set("a", "1");
    store.set("b", "2");
    store.get("a");
    store.set("c", "3");
    EXPECT_EQ(store.get("a"), "1");
    EXPECT_EQ(store.get("b"), std::nullopt);
}

TEST(KeyValueStoreConcurrencyTest, ConcurrentMixedOperations) {
    ManualClock clock;
    KeyValueStore store(64, clock);
    constexpr int kThreads = 8;
    constexpr int kOps = 5'000;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&store, &clock, t] {
            for (int i = 0; i < kOps; ++i) {
                const std::string key = "k" + std::to_string((i * 7 + t) % 100);
                switch (i % 6) {
                    case 0: store.set(key, std::to_string(i)); break;
                    case 1: store.set(key, "x", 5ms); break;
                    case 2: store.get(key); break;
                    case 3: store.expire(key, 3ms); break;
                    case 4: store.del(key); break;
                    default:
                        clock.advance(1ms);
                        store.purgeExpired(16);
                        break;
                }
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }

    EXPECT_LE(store.size(), 64u);
    clock.advance(1h);
    while (store.purgeExpired(1000) > 0) {
    }
    for (int k = 0; k < 100; ++k) {
        const std::string key = "k" + std::to_string(k);
        if (store.get(key)) {
            EXPECT_EQ(store.pttl(key), KeyValueStore::kNoExpiry) << key;
        }
    }
}

}  // namespace
}  // namespace miniredis::store
