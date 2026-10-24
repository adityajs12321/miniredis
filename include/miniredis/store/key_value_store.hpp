#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "miniredis/cache/lru_cache.hpp"
#include "miniredis/store/clock.hpp"

namespace miniredis::store {

class KeyValueStore {
public:
    using Millis = std::chrono::milliseconds;

    static constexpr std::int64_t kNoKey = -2;
    static constexpr std::int64_t kNoExpiry = -1;

    KeyValueStore(long long capacity, const Clock& clock);

    KeyValueStore(const KeyValueStore&) = delete;
    KeyValueStore& operator=(const KeyValueStore&) = delete;

    std::optional<std::string> get(const std::string& key);

    void set(std::string key, std::string value, std::optional<Millis> ttl = std::nullopt);

    bool del(const std::string& key);

    bool expire(const std::string& key, Millis ttl);

    std::int64_t pttl(const std::string& key);

    std::size_t purgeExpired(std::size_t maxKeys);

    std::size_t size() const;

private:
    using TimePoint = Clock::TimePoint;

    struct Entry {
        std::string value;
        std::optional<TimePoint> expiresAt;
    };

    Entry* findLive(const std::string& key, TimePoint now);

    void remove(const std::string& key, const Entry& entry);

    void unindex(const std::string& key, const Entry& entry);

    const Clock& clock_;
    mutable std::mutex mutex_;
    cache::LRUCache<std::string, Entry> entries_;
    std::set<std::pair<TimePoint, std::string>> expiryIndex_;
};

}  // namespace miniredis::store
