#pragma once

#include<list>
#include<memory>
#include<unordered_map>
#include<mutex>
#include"Cachepolicy.h"

namespace CacheDemo
{

template<typename Key, typename Value>
class FIFOCache : public Cachepolicy<Key, Value>
{

public:
    using Nodetype = std::pair<Key,Value>; 
    using Listtype = std::list<Nodetype>;
    using ListIterator = typename Listtype::iterator;
    using Hashmap = std::unordered_map<Key, ListIterator>;

    explicit FIFOCache(size_t cap);

    ~FIFOCache() override;

    void put(const Key& key, const Value& value) override;

    bool get(const Key& key, Value& value) override;

    void deletenode(const Key& key);

private:
    /* data */
    size_t capacity_;
    Listtype Cachelist_;
    Hashmap Cachemap_;
    std::mutex fifomutex_;
};

} // namespace CacheDemo

#include "../src/FIFOCache.tpp" 