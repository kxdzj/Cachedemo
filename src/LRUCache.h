#pragma once

#include<list>
#include<memory>
#include<thread>
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
            Cachemap_.erase(tail.first);
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
class LRUKCache : public LRUCache<Key, Value> {
public:
    LRUKCache(size_t cap, size_t historycap, int k)
        : LRUCache<Key, Value>(cap),
          historylist_(std::make_unique<LRUCache<Key, size_t>>(historycap)),
          k_(k) {}

    void put(const Key& key, const Value& value) override
    {
        Value tempvalue;
        if (LRUCache<Key, Value>::get(key, tempvalue)) {
            LRUCache<Key, Value>::put(key, value);
            return;
        }

        size_t historycount = 0;
        std::lock_guard<std::mutex> lock(historymutex_);

        if (!historylist_->get(key, historycount)) {
            historycount = 1;
        } else {
            historycount++;
        }

        historylist_->put(key, historycount);

        if (historycount >= k_) {
            historylist_->deletenode(key);
            LRUCache<Key, Value>::put(key, value);
        }
    }

    bool get(const Key& key, Value& value) override
    {
    size_t historycount = 0;
    std::lock_guard<std::mutex> lock(historymutex_);

    // 先查历史记录，增加访问次数
    if (historylist_->get(key, historycount)) {
        historylist_->put(key, ++historycount);  // 访问次数 +1
    } else {
        historylist_->put(key, 1);  // 第一次访问
    }

    // 再查 LRUCache 是否有数据
    return LRUCache<Key, Value>::get(key, value);
    }


private:
    int k_;  
    std::unique_ptr<LRUCache<Key, size_t>> historylist_;
    std::mutex historymutex_;
};


template<typename Key, typename Value>
class HashLRUCache
{
private:
   // 将key转换为对应hash值
   size_t Hash(Key key)
   {
       std::hash<Key> hashFunc;
       return hashFunc(key);
   }

private:
    size_t    capacity_;  // 总容量
    int       sliceNum_;  // 切片数量
    std::vector<std::unique_ptr<LRUCache<Key, Value>>> lruSliceCaches_; // 切片LRU缓存

public:

   HashLRUCache(size_t capacity, int slicenum):
   capacity_(capacity),
   sliceNum_(slicenum > 0 ? slicenum : std::thread::hardware_concurrency())
   {
    // 获取每个分片的大小
    size_t sliceSize = std::ceil(capacity / static_cast<double>(sliceNum_)); 
        for (int i = 0; i < sliceNum_; ++i)
        {
            lruSliceCaches_.emplace_back(new LRUCache<Key, Value>(sliceSize)); 
        }
   }

   void put(const Key& key, const Value& value)
   {
       // 获取key的hash值，并计算出对应的分片索引
       size_t sliceIndex = Hash(key) % sliceNum_;
       return lruSliceCaches_[sliceIndex]->put(key, value);
   }

   bool get(const Key& key, Value& value)
   {
       // 获取key的hash值，并计算出对应的分片索引
       size_t sliceIndex = Hash(key) % sliceNum_;
       return lruSliceCaches_[sliceIndex]->get(key, value);
   }

   Value get(const Key& key)
   {
       Value value;
       memset(&value, 0, sizeof(value));
       get(key, value);
       return value;
   }

};

} // namespace CacheDemo