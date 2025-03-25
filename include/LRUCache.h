#pragma once

#include <cmath>
#include <list>
#include <vector>
#include <memory>
#include <thread>
#include <unordered_map>
#include <mutex>
#include "Cachepolicy.h"

namespace CacheDemo {

template<typename Key, typename Value>
class LRUCache : public Cachepolicy<Key, Value>
{
public:
    using Nodetype = std::pair<Key, Value>;
    using Listtype = std::list<Nodetype>;
    using ListIterator = typename Listtype::iterator;
    using Hashmap = std::unordered_map<Key, ListIterator>;

    explicit LRUCache(size_t cap);
    ~LRUCache() override;

    void put(const Key& key, const Value& value) override;
    bool get(const Key& key, Value& value) override;
    void deletenode(const Key& key);

private:
    size_t capacity_;
    Listtype Cachelist_;
    Hashmap Cachemap_;
    std::mutex LRUmutex_;
};

template<typename Key, typename Value>
class LRUKCache : public LRUCache<Key, Value> {
public:
    LRUKCache(size_t cap, size_t historycap, int k);

    void put(const Key& key, const Value& value) override;
    bool get(const Key& key, Value& value) override;

private:
    int k_;
    std::unique_ptr<LRUCache<Key, size_t>> historylist_;
    std::mutex historymutex_;
};

template<typename Key, typename Value>
class HashLRUCache
{
public:
    HashLRUCache(size_t capacity, int slicenum);

    void put(const Key& key, const Value& value);
    bool get(const Key& key, Value& value);
    Value get(const Key& key);

private:
    size_t capacity_;
    int sliceNum_;
    std::vector<std::unique_ptr<LRUCache<Key, Value>>> lruSliceCaches_;
    size_t Hash(Key key);
};

} // namespace CacheDemo

#include "../src/LRUCache.tpp"  