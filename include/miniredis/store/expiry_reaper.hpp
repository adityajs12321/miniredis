#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>

#include "miniredis/store/key_value_store.hpp"

namespace miniredis::store {

class ExpiryReaper {
public:
    static constexpr std::size_t kBatchSize = 256;

    ExpiryReaper(KeyValueStore& store, std::chrono::milliseconds interval);
    ~ExpiryReaper();

    ExpiryReaper(const ExpiryReaper&) = delete;
    ExpiryReaper& operator=(const ExpiryReaper&) = delete;

    void stop();

private:
    void run();

    KeyValueStore& store_;
    const std::chrono::milliseconds interval_;
    std::mutex mutex_;
    std::condition_variable wakeUp_;
    bool stopping_ = false;
    std::thread thread_;
};

}  // namespace miniredis::store
