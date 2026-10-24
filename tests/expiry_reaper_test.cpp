#include "miniredis/store/expiry_reaper.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "manual_clock.hpp"

namespace miniredis::store {
namespace {

using namespace std::chrono_literals;
using miniredis::testing::ManualClock;

template <typename Pred>
bool eventually(Pred pred, std::chrono::milliseconds timeout = 5s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(1ms);
    }
    return pred();
}

TEST(ExpiryReaperTest, PurgesExpiredKeysNobodyReads) {
    ManualClock clock;
    KeyValueStore store(10'000, clock);
    for (int i = 0; i < 1'000; ++i) {
        store.set("k" + std::to_string(i), "v", 100ms);
    }
    store.set("forever", "v");

    ExpiryReaper reaper(store, 1ms);
    clock.advance(100ms);

    EXPECT_TRUE(eventually([&] { return store.size() == 1; }));
    EXPECT_EQ(store.get("forever"), "v");
}

TEST(ExpiryReaperTest, LeavesUnexpiredKeys) {
    ManualClock clock;
    KeyValueStore store(10, clock);
    store.set("k", "v", 100ms);

    ExpiryReaper reaper(store, 1ms);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(store.size(), 1u);
}

TEST(ExpiryReaperTest, StopIsPromptAndIdempotent) {
    ManualClock clock;
    KeyValueStore store(10, clock);
    ExpiryReaper reaper(store, 1h);

    const auto start = std::chrono::steady_clock::now();
    reaper.stop();
    reaper.stop();
    EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
}

TEST(ExpiryReaperTest, RejectsNonPositiveInterval) {
    ManualClock clock;
    KeyValueStore store(10, clock);
    EXPECT_THROW(ExpiryReaper(store, 0ms), std::invalid_argument);
}

}  // namespace
}  // namespace miniredis::store
