#include "miniredis/store/expiry_reaper.hpp"

#include <stdexcept>

namespace miniredis::store {

ExpiryReaper::ExpiryReaper(KeyValueStore& store, std::chrono::milliseconds interval)
    : store_(store), interval_(interval) {
    if (interval.count() <= 0) {
        throw std::invalid_argument("reaper interval must be positive");
    }
    thread_ = std::thread(&ExpiryReaper::run, this);
}

ExpiryReaper::~ExpiryReaper() {
    stop();
}

void ExpiryReaper::stop() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    wakeUp_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void ExpiryReaper::run() {
    std::unique_lock lock(mutex_);
    while (!wakeUp_.wait_for(lock, interval_, [this] { return stopping_; })) {
        lock.unlock();
        while (store_.purgeExpired(kBatchSize) == kBatchSize) {
        }
        lock.lock();
    }
}

}  // namespace miniredis::store
