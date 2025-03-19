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

    explicit  FIFOCache(size_t cap):capacity_(cap) 
    {
        Cachemap_.reserve(cap); 
    }

    ~FIFOCache() override = default;

    void put(const Key& key, const Value& value) override
    {
        if(capacity_ == 0) return;

        std::lock_guard<std::mutex> lock(fifomutex_);

        auto it = Cachemap_.find(key);
        if(it != Cachemap_.end()){
            it->second->second = value;
            return;
        }

        if(Cachelist_.size() >= capacity_){
            auto tail = Cachelist_.back();
            Cachemap_.erase(tail->first);
            Cachelist_.pop_back();
        }

        Cachelist_.push_front({key, value});
        Cachemap_[key] = Cachelist_.begin();
    }

    bool get(const Key& key, Value& value) override
    {
        std::lock_guard<std::mutex> lock(fifomutex_);

        auto it = Cachemap_.find(key);
        if(it == Cachemap_.end()){
            return false;
        }

        value = it->second->second;
        return true;

    }
    void deletenode(const Key& key){
        std::lock_guard<std::mutex> lock(fifomutex_);

        auto it = Cachemap_.find(key);
        if(it == Cachemap_.end()){
            return;
        }

        Cachelist_.erase(it->second);
        Cachemap_.erase(it);

    }

private:
    /* data */
    size_t capacity_;
    Listtype Cachelist_;
    Hashmap Cachemap_;
    std::mutex fifomutex_;
};

} // namespace CacheDemo