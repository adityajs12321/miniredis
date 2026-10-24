#include "miniredis/store/key_value_store.hpp"

#include <stdexcept>

namespace miniredis::store {

KeyValueStore::KeyValueStore(long long capacity, const Clock& clock)
    : clock_(clock), entries_(capacity) {}


std::optional<std::string> KeyValueStore::get(const std::string& key) {
    std::lock_guard lock(mutex_);
    const Entry* entry = findLive(key, clock_.now());
    return entry ? std::optional<std::string>(entry->value) : std::nullopt;
}

void KeyValueStore::set(std::string key, std::string value, std::optional<Millis> ttl) {
    if (ttl && ttl->count() <= 0) {
        throw std::invalid_argument("ttl must be positive");
    }
    std::lock_guard lock(mutex_);
    const TimePoint now = clock_.now();

    // Drop the old deadline before overwriting
    if (const Entry* old = entries_.peek(key)) {
        unindex(key, *old);
    }

    // Insert, unindexing anything the LRU evicts
    Entry entry{std::move(value), ttl ? std::optional<TimePoint>(now + *ttl) : std::nullopt};
    if (entry.expiresAt) {
        expiryIndex_.emplace(*entry.expiresAt, key);
    }
    if (auto evicted = entries_.put(std::move(key), std::move(entry))) {
        unindex(evicted->first, evicted->second);
    }
}

bool KeyValueStore::del(const std::string& key) {
    std::lock_guard lock(mutex_);
    const Entry* entry = findLive(key, clock_.now());
    if (!entry) {
        return false;
    }
    remove(key, *entry);
    return true;
}

bool KeyValueStore::expire(const std::string& key, Millis ttl) {
    std::lock_guard lock(mutex_);
    const TimePoint now = clock_.now();
    Entry* entry = findLive(key, now);
    if (!entry) {
        return false;
    }
    if (ttl.count() <= 0) {
        remove(key, *entry);
        return true;
    }
    unindex(key, *entry);
    entry->expiresAt = now + ttl;
    expiryIndex_.emplace(*entry->expiresAt, key);
    return true;
}

std::int64_t KeyValueStore::pttl(const std::string& key) {
    std::lock_guard lock(mutex_);
    const TimePoint now = clock_.now();
    const Entry* entry = findLive(key, now);
    if (!entry) {
        return kNoKey;
    }
    if (!entry->expiresAt) {
        return kNoExpiry;
    }
    return std::chrono::duration_cast<Millis>(*entry->expiresAt - now).count();
}

std::size_t KeyValueStore::purgeExpired(std::size_t maxKeys) {
    std::lock_guard lock(mutex_);
    const TimePoint now = clock_.now();
    std::size_t removed = 0;
    while (removed < maxKeys && !expiryIndex_.empty() && expiryIndex_.begin()->first <= now) {
        auto node = expiryIndex_.extract(expiryIndex_.begin());
        entries_.erase(node.value().second);
        ++removed;
    }
    return removed;
}

std::size_t KeyValueStore::size() const {
    std::lock_guard lock(mutex_);
    return entries_.size();
}

// --- Internal helpers ---

KeyValueStore::Entry* KeyValueStore::findLive(const std::string& key, TimePoint now) {
    Entry* entry = entries_.find(key);
    if (entry && entry->expiresAt && *entry->expiresAt <= now) {
        remove(key, *entry);
        return nullptr;
    }
    return entry;
}

void KeyValueStore::remove(const std::string& key, const Entry& entry) {
    unindex(key, entry);
    entries_.erase(key);
}

void KeyValueStore::unindex(const std::string& key, const Entry& entry) {
    if (entry.expiresAt) {
        expiryIndex_.erase({*entry.expiresAt, key});
    }
}

}  // namespace miniredis::store
