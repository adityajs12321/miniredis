#include "miniredis/concurrency/thread_pool.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>

#include <gtest/gtest.h>

namespace miniredis::concurrency {
namespace {

using namespace std::chrono_literals;

TEST(ThreadPoolTest, RunsEverySubmittedTaskBeforeShutdownReturns) {
    std::atomic<int> count{0};
    ThreadPool pool(4);
    for (int i = 0; i < 1'000; ++i) {
        pool.submit([&count] { ++count; });
    }
    pool.shutdown();
    EXPECT_EQ(count.load(), 1'000);
}

TEST(ThreadPoolTest, RunsTasksConcurrently) {
    constexpr int kThreads = 4;
    ThreadPool pool(kThreads);
    std::mutex mutex;
    std::condition_variable allArrived;
    int arrived = 0;
    bool timedOut = false;

    for (int i = 0; i < kThreads; ++i) {
        pool.submit([&] {
            std::unique_lock lock(mutex);
            ++arrived;
            allArrived.notify_all();
            if (!allArrived.wait_for(lock, 5s, [&] { return arrived == kThreads; })) {
                timedOut = true;
            }
        });
    }
    pool.shutdown();
    EXPECT_FALSE(timedOut);
    EXPECT_EQ(arrived, kThreads);
}

TEST(ThreadPoolTest, ThrowingTaskDoesNotKillWorker) {
    ThreadPool pool(1);
    std::atomic<bool> ran{false};
    pool.submit([] { throw std::runtime_error("boom"); });
    pool.submit([&ran] { ran = true; });
    pool.shutdown();
    EXPECT_TRUE(ran.load());
}

TEST(ThreadPoolTest, SubmitAfterShutdownThrows) {
    ThreadPool pool(2);
    pool.shutdown();
    pool.shutdown();
    EXPECT_THROW(pool.submit([] {}), std::runtime_error);
}

TEST(ThreadPoolTest, DestructorDrainsQueue) {
    std::atomic<int> count{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 100; ++i) {
            pool.submit([&count] {
                std::this_thread::sleep_for(100us);
                ++count;
            });
        }
    }
    EXPECT_EQ(count.load(), 100);
}

TEST(ThreadPoolTest, RejectsZeroThreads) {
    EXPECT_THROW(ThreadPool(0), std::invalid_argument);
}

}  // namespace
}  // namespace miniredis::concurrency
