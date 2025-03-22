#pragma once

#include <list>
#include <memory>
#include <thread>
#include <unordered_map>
#include <mutex>
#include "Cachepolicy.h"

namespace CacheDemo {

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
            get(key, value); // 增加访问频率
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
};

template <typename Key, typename Value>
class LFUMCache : public Cachepolicy<Key, Value>{
public:
    using Listtype = std::list<Key>;
    using Hashmap = std::unordered_map<Key, std::pair<Value, int>>;
    using Freqmap = std::unordered_map<int, Listtype>;
    using ListIterator = typename Listtype::iterator;
    using Itermap = std::unordered_map<Key, ListIterator>;

    explicit LFUMCache(size_t cap, int max_freq = 10) : capacity_(cap), max_freq_(max_freq), min_freq_(0), put_count_(0) {}

    ~LFUMCache() = default;

    void put(const Key& key, const Value& value) {
        if (capacity_ == 0) return;

        std::lock_guard<std::mutex> lock(LFUmutex_);

        if (cache_.count(key)) {
            cache_[key].first = value;
            get(key, value); // 增加访问频率
            return;
        }

        if (cache_.size() >= capacity_) {
            applyDecay(); // 先执行衰减，看看是否能腾出空间
            evictLFU();   // 仍然满了就淘汰最少使用的数据
        }

        min_freq_ = 1;
        cache_[key] = {value, 1};
        freq_map_[1].push_front(key);
        iter_map_[key] = freq_map_[1].begin();

        put_count_++;
    }

    bool get(const Key& key, Value& value) {
        std::lock_guard<std::mutex> lock(LFUmutex_);

        if (!cache_.count(key)) return false;

        value = cache_[key].first;
        int freq = cache_[key].second;

        freq_map_[freq].erase(iter_map_[key]);
        if (freq_map_[freq].empty()) {
            freq_map_.erase(freq);
            if (min_freq_ == freq) min_freq_++;
        }

        freq = std::min(freq + 1, max_freq_); // 频次不能超过 max_freq_
        cache_[key].second = freq;
        freq_map_[freq].push_front(key);
        iter_map_[key] = freq_map_[freq].begin();

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
    int max_freq_; // 访问频次的最大值，防止无限增长
    int min_freq_;
    Hashmap cache_;
    Freqmap freq_map_;
    Itermap iter_map_;
    std::mutex LFUmutex_;
    int put_count_; // 记录 put() 的调用次数，用于触发频次衰减

    void evictLFU() {
        if (freq_map_.empty()) return;

        Key evict_key = freq_map_[min_freq_].back();
        freq_map_[min_freq_].pop_back();
        if (freq_map_[min_freq_].empty()) freq_map_.erase(min_freq_);

        cache_.erase(evict_key);
        iter_map_.erase(evict_key);
    }

    void applyDecay() {
        if (put_count_ != 0 && put_count_ % capacity_ != 0) return; // 每次 put() 满足一定条件才执行衰减

        std::unordered_map<int, Listtype> new_freq_map;
        for (auto& [freq, keys] : freq_map_) {
            int new_freq = std::max(1, freq / 2); // 频次衰减，至少降到 1
            new_freq_map[new_freq].splice(new_freq_map[new_freq].end(), keys);
        }
        freq_map_ = std::move(new_freq_map);

        for (auto& [key, pair] : cache_) {
            pair.second = std::max(1, pair.second / 2); // 更新缓存中的频次值
        }

        put_count_ = 0; // 重置 put 计数
    }
};


















}  // namespace CacheDemo
