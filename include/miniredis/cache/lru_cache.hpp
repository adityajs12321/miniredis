#pragma once

#include <cstddef>
#include <list>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace miniredis::cache {

template <typename K, typename V, typename Hash = std::hash<K>>
class LRUCache {
public:
    explicit LRUCache(long long capacity) : capacity_(checkCapacity(capacity)) {}

    std::optional<V> get(const K& key) {
        V* value = find(key);
        return value ? std::optional<V>(*value) : std::nullopt;
    }

    V* find(const K& key) {
        auto it = index_.find(key);
        if (it == index_.end()) {
            return nullptr;
        }
        touch(it->second);
        return &it->second->second;
    }

    const V* peek(const K& key) const {
        auto it = index_.find(key);
        return it == index_.end() ? nullptr : &it->second->second;
    }

    std::optional<std::pair<K, V>> put(K key, V value) {
        // Existing key: update in place
        if (auto it = index_.find(key); it != index_.end()) {
            it->second->second = std::move(value);
            touch(it->second);
            return std::nullopt;
        }

        // New key: evict the least recently used entry if full
        std::optional<std::pair<K, V>> evicted;
        if (index_.size() >= capacity_) {
            index_.erase(entries_.front().first);
            evicted = std::move(entries_.front());
            entries_.pop_front();
        }

        entries_.emplace_back(std::move(key), std::move(value));
        index_.emplace(entries_.back().first, std::prev(entries_.end()));
        return evicted;
    }

    bool erase(const K& key) {
        auto it = index_.find(key);
        if (it == index_.end()) {
            return false;
        }
        entries_.erase(it->second);
        index_.erase(it);
        return true;
    }

    std::size_t size() const noexcept { return index_.size(); }

    std::size_t capacity() const noexcept { return capacity_; }

private:
    using Entries = std::list<std::pair<K, V>>;

    static std::size_t checkCapacity(long long capacity) {
        if (capacity <= 0) {
            throw std::invalid_argument("capacity must be positive: " + std::to_string(capacity));
        }
        return static_cast<std::size_t>(capacity);
    }

    void touch(typename Entries::iterator entry) {
        entries_.splice(entries_.end(), entries_, entry);
    }

    std::size_t capacity_;
    Entries entries_;
    std::unordered_map<K, typename Entries::iterator, Hash> index_;
};

}  // namespace miniredis::cache
