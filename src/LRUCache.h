#pragma once

#include<list>
#include<memory>
#include<unordered_map>
#include<mutex>
#include"Cachepolicy.h"

namespace CacheDemo
{

template<typename Key, typename Value>
class LRUCache : public Cachepolicy<Key, Value>
{

public:
    using Nodetype = std::pair<Key,Value>; 
    using Listtype = std::list<Nodetype>;
    using ListIterator = typename Listtype::iterator;
    using Hashmap = std::unordered_map<Key, ListIterator>;

    explicit  LRUCache(size_t cap):capacity_(cap) 
    {
        Cachemap_.reserve(cap); 
    }

    ~LRUCache() override = default;

    void put(const Key& key, const Value& value) override
    {
        if(capacity_ == 0) return;

        std::lock_guard<std::mutex> lock(LRUmutex_);

        auto it = Cachemap_.find(key);
        if(it != Cachemap_.end()){
            Cachelist_.erase(it->second);
            Cachemap_.erase(it);
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
        std::lock_guard<std::mutex> lock(LRUmutex_);

        auto it = Cachemap_.find(key);
        if(it == Cachemap_.end()){
            return false;
        }
        
        value = it->second->second;
        Cachelist_.splice(Cachelist_.begin(), Cachelist_, it->second);

        return true;
    }

    void deletenode(const Key& key){
        std::lock_guard<std::mutex> lock(LRUmutex_);

        auto it = Cachemap_.find(key);
        if(it == Cachemap_.end()){
            return;
        }

        Cachelist_.erase(it->second);
        Cachemap_.erase(it);

    }

private:

    size_t capacity_;
    Listtype Cachelist_;
    Hashmap Cachemap_;
    std::mutex LRUmutex_;
};


template<typename Key, typename Value>
class LRUKCache : public LRUCache<Key, Value>{
public:
    LRUKCache(size_t cap, size_t historycap, int k)
        :LRUCache<Key, Value>(cap),
        historylist_(std::make_unique<LRUCache<Key, size_t>>(historycap)),
        k_(k)
        {}

    void put(const Key& key, const Value& value) override
    {
        Value tempvalue;
        // 直接在缓存中则覆盖，否则查历史
        if(LRUCache<Key, Value>::get(key, tempvalue)){
            LRUCache<Key, Value>::put(key, value);
            return;
        }
        size_t historycount = 0;

        std::lock_guard<std::mutex> lock(historymutex_);

        historylist_->get(key, historycount);
        historylist_->put(key, ++historycount);
        if(historycount >= k_){
            historylist_->deletenode(key);
            LRUCache<Key, Value>::put(key, value);
        }
    }

    bool get(const Key& key, Value& value) override
    {
        size_t historycount = 0;
        std::lock_guard<std::mutex> lock(historymutex_);
        // 在历史记录里找到，就说明一定不在缓存，更新次数返回错误即可
        if(historylist_->get(key, historycount)){
            historylist_->put(key, ++historycount);
            return false;
        }
        // 没找到也要更新次数，然后去缓存里找
        historylist_->put(key, ++historycount);
        return LRUCache<Key, Value>::get(key, value);
    }

private:
    // 超过k次才放入缓存
    int k_; 
    // 历史记录也需要符合LRU，只需要记录键和频次  
    std::unique_ptr<LRUCache<Key, size_t>> historylist_;
    // 历史记录锁
    std::mutex historymutex_;
};















} // namespace CacheDemo