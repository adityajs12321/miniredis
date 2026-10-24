#include "miniredis/concurrency/thread_pool.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>

namespace miniredis::concurrency {

ThreadPool::ThreadPool(std::size_t threads) {
    if (threads == 0) {
        throw std::invalid_argument("thread pool needs at least 1 thread");
    }
    workers_.reserve(threads);
    try {
        for (std::size_t i = 0; i < threads; ++i) {
            workers_.emplace_back(&ThreadPool::workerLoop, this);
        }
    } catch (...) {
        shutdown();
        throw;
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::submit(Task task) {
    {
        std::lock_guard lock(mutex_);
        if (stopping_) {
            throw std::runtime_error("thread pool is shut down");
        }
        tasks_.push_back(std::move(task));
    }
    taskAvailable_.notify_one();
}

void ThreadPool::shutdown() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    taskAvailable_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::workerLoop() {
    while (true) {
        Task task;
        {
            std::unique_lock lock(mutex_);
            taskAvailable_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
            if (tasks_.empty()) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop_front();
        }
        try {
            task();
        } catch (const std::exception& e) {
            std::cerr << "thread pool: task threw: " << e.what() << '\n';
        } catch (...) {
            std::cerr << "thread pool: task threw an exception\n";
        }
    }
}

}  // namespace miniredis::concurrency
