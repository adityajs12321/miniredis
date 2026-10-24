#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace miniredis::concurrency {

class ThreadPool {
public:
    using Task = std::function<void()>;

    explicit ThreadPool(std::size_t threads);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void submit(Task task);

    void shutdown();

    std::size_t threadCount() const noexcept { return workers_.size(); }

private:
    void workerLoop();

    std::mutex mutex_;
    std::condition_variable taskAvailable_;
    std::deque<Task> tasks_;
    bool stopping_ = false;
    std::vector<std::thread> workers_;
};

}  // namespace miniredis::concurrency
