#pragma once

#include <list>
#include <cmath>
#include <vector>
#include <memory>
#include <thread>
#include <unordered_map>
#include <mutex>
#include "Cachepolicy.h"

namespace CacheDemo {

// 限制的最大频次
constexpr int MAX_FREQ = 16;


template <typename Key, typename Value>
class LFUCache : public Cachepolicy<Key, Value> {
public:

    // 用于存储相同访问频次的键的双向链表
    using Listtype = std::list<Key>;
    // 存储缓存的键值对，同时记录访问频次
    using Hashmap = std::unordered_map<Key, std::pair<Value, int>>; 
    // 频次字段映射到对应双向链表，方便o1找到最少使用
    using Freqmap = std::unordered_map<int, Listtype>; 
    using ListIterator = typename Listtype::iterator;
    // 方便o1删除，因为可以直接拿到迭代器
    using Itermap = std::unordered_map<Key, ListIterator>; 

    explicit LFUCache(size_t cap) : capacity_(cap), min_freq_(0) {}

    ~LFUCache() override = default;

    void put(const Key& key, const Value& value) override {
        if (capacity_ == 0) return;
        
        std::lock_guard<std::mutex> lock(LFUmutex_);

        // 如果key已存在，则更新value并增加访问频率
        if (cache_.count(key)) {
            cache_[key].first = value;
            increase_frequency(key);
            return;
        }

        // 如果缓存已满，淘汰访问频率最低的 key
        if (cache_.size() >= capacity_) {
            Key evict_key = freq_map_[min_freq_].back();
            freq_map_[min_freq_].pop_back();
            if (freq_map_[min_freq_].empty()) freq_map_.erase(min_freq_);

            cache_.erase(evict_key);
            iter_map_.erase(evict_key);
        }

        // 插入新 key，访问频率设为 1
        min_freq_ = 1;
        cache_[key] = {value, 1};
        freq_map_[1].push_front(key);
        iter_map_[key] = freq_map_[1].begin();
    }

    bool get(const Key& key, Value& value) override {
        std::lock_guard<std::mutex> lock(LFUmutex_);

        if (!cache_.count(key)) return false;

        // 获取 value
        value = cache_[key].first;

        increase_frequency(key);

        return true;
    }

    void deletenode(const Key& key) {
        std::lock_guard<std::mutex> lock(LFUmutex_);

        if (!cache_.count(key)) return;

        int freq = cache_[key].second;

        freq_map_[freq].erase(iter_map_[key]);
        if (freq_map_[freq].empty()) {
            freq_map_.erase(freq);
            if (min_freq_ == freq) min_freq_++;
        }

        cache_.erase(key);
        iter_map_.erase(key);
    }

private:
    size_t capacity_;
    int min_freq_; 
    // key->(value,freq)
    Hashmap cache_; 
    // freq->list
    Freqmap freq_map_; 
    // key->(list::iterator)
    Itermap iter_map_; 

    std::mutex LFUmutex_;

private:
void increase_frequency(const Key& key) {
    int freq = cache_[key].second;
    // 从当前频率列表中移除key
    freq_map_[freq].erase(iter_map_[key]);
    // 当前频率列表为空，则最小访问次数应该+1
    if (freq_map_[freq].empty()) {
        freq_map_.erase(freq);
        if (min_freq_ == freq) ++min_freq_;
    }
    // 增加 key 的访问频率
    ++freq;
    cache_[key].second = freq;
    freq_map_[freq].push_front(key);
    iter_map_[key] = freq_map_[freq].begin();
}
};

template <typename Key, typename Value>
class LFUMCache : public Cachepolicy<Key, Value> {
public:
    using Listtype = std::list<Key>;
    using Hashmap = std::unordered_map<Key, std::pair<Value, int>>;
    using Freqmap = std::unordered_map<int, Listtype>;
    using ListIterator = typename Listtype::iterator;
    using Itermap = std::unordered_map<Key, ListIterator>;

    explicit LFUMCache(size_t cap, int max_freq = MAX_FREQ)
        : capacity_(cap), max_freq_(max_freq), min_freq_(0), put_count_(0) {}

    ~LFUMCache() override = default;

    void put(const Key& key, const Value& value) override {
        if (capacity_ == 0) return;
        std::lock_guard<std::mutex> lock(LFUmutex_);

        if (cache_.count(key)) {
            cache_[key].first = value;
            increase_frequency(key);
            return;
        }

        if (cache_.size() >= capacity_) {
            freqDecay();
            evictLFU();
        }

        min_freq_ = 1;
        cache_[key] = {value, 1};
        freq_map_[1].push_front(key);
        iter_map_[key] = freq_map_[1].begin();
        put_count_++;
    }

    bool get(const Key& key, Value& value) override {
        std::lock_guard<std::mutex> lock(LFUmutex_);
        if (!cache_.count(key)) return false;
        value = cache_[key].first;
        increase_frequency(key);
        return true;
    }

    void deletenode(const Key& key)  {
        std::lock_guard<std::mutex> lock(LFUmutex_);
        if (!cache_.count(key)) return;

        int freq = cache_[key].second;

        auto it = iter_map_[key];
        iter_map_.erase(key);
        freq_map_[freq].erase(it);

        if (freq_map_[freq].empty()) {
            freq_map_.erase(freq);
            if (min_freq_ == freq) min_freq_++;
        }

        cache_.erase(key);
    }

    void purge(){
        std::lock_guard<std::mutex> lock(LFUmutex_);
        cache_.clear();
        iter_map_.clear();
        freq_map_.clear();
        min_freq_ = 0;
        put_count_ = 0;
    }

private:
    size_t capacity_;
    int max_freq_;
    int min_freq_;
    Hashmap cache_;
    Freqmap freq_map_;
    Itermap iter_map_;
    std::mutex LFUmutex_;
    int put_count_;

    void evictLFU() {
        if (freq_map_.empty()) return;

        Key evict_key = freq_map_[min_freq_].back();
        iter_map_.erase(evict_key);
        freq_map_[min_freq_].pop_back();

        if (freq_map_[min_freq_].empty()) {
            freq_map_.erase(min_freq_);
        }

        cache_.erase(evict_key);
    }

    void freqDecay() {
        if (freq_map_.empty()) return;
        if (put_count_ != 0 && put_count_ % capacity_ != 0) return;

        std::unordered_map<int, Listtype> new_freq_map;

        for (auto& [freq, keys] : freq_map_) {
            int new_freq = std::max(1, freq / 2);
            new_freq_map[new_freq].splice(new_freq_map[new_freq].end(), keys);
        }

        for (auto& [key, pair] : cache_) {
            pair.second = std::max(1, pair.second / 2);
        }

        freq_map_ = std::move(new_freq_map);

        iter_map_.clear();
        for (auto& [freq, keys] : freq_map_) {
            for (auto it = keys.begin(); it != keys.end(); ++it) {
                iter_map_[*it] = it;
            }
        }

        put_count_ = 0;
    }

    void increase_frequency(const Key& key) {
        int freq = cache_[key].second;

        freq_map_[freq].erase(iter_map_[key]);

        if (freq_map_[freq].empty()) {
            freq_map_.erase(freq);
            if (min_freq_ == freq) min_freq_++;
        }

        freq = std::min(freq + 1, max_freq_);
        cache_[key].second = freq;

        freq_map_[freq].push_front(key);
        iter_map_[key] = freq_map_[freq].begin();
    }
};

template<typename Key, typename Value>
class HashLFUCache
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
    std::vector<std::unique_ptr<LFUMCache<Key, Value>>> lfuSliceCaches_;

public:

   HashLFUCache(size_t capacity, int sliceNum):
   capacity_(capacity),
   sliceNum_(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency())
   {
        size_t sliceSize = std::ceil(capacity_ / static_cast<double>(sliceNum_));
        for (int i = 0; i < sliceNum_; ++i)
        {
            lfuSliceCaches_.emplace_back(std::make_unique<LFUMCache<Key, Value> >(sliceSize)); 
        }
   }


   void put(const Key& key, const Value& value)
   {
       // 获取key的hash值，并计算出对应的分片索引
       size_t sliceIndex = Hash(key) % sliceNum_;
       lfuSliceCaches_[sliceIndex]->put(key, value);
   }

   bool get(const Key& key, Value& value)
   {
       // 获取key的hash值，并计算出对应的分片索引
       size_t sliceIndex = Hash(key) % sliceNum_;
       return lfuSliceCaches_[sliceIndex]->get(key, value);
   }

   Value get(const Key& key)
   {
       Value value{};
       get(key, value);
       return value;
   }

    // 清除缓存
    void purge()
    {
        for (auto& lfuSliceCache : lfuSliceCaches_)
        {
            lfuSliceCache->purge();
        }
    }

};// class HashLFUCache


}  // namespace CacheDemo
