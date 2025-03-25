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

    explicit LFUCache(size_t cap);

    ~LFUCache() override;

    void put(const Key& key, const Value& value) override;
    bool get(const Key& key, Value& value) override;
    void deletenode(const Key& key);

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

    void increase_frequency(const Key& key); 
};

// Node
template<typename Key, typename Value>
struct LRUNode {
  Key key;
  Value value;
  size_t freq;
  
  LRUNode(Key k, Value v, size_t f = 1)
      : key(k), value(v), freq(f) {}
};

template <typename Key, typename Value>
class LFUMCache : public Cachepolicy<Key, Value> {
public:

    using Listtype = std::list<LRUNode<Key, Value>>;
    using ListIterator = typename Listtype::iterator;
    using Freqmap = std::unordered_map<size_t, Listtype>;
    using Cachemap = std::unordered_map<Key, ListIterator>;

    explicit LFUMCache(size_t cap, int max_freq = MAX_FREQ);
    ~LFUMCache() override;

    void put(const Key& key, const Value& value) override;

    bool get(const Key& key, Value& value) override;

private:
    size_t capacity_;
    int max_freq_;
    int min_freq_;
    Cachemap cache_;  //  key->list<node>iterator
    Freqmap freq_map_; // freq->list<node>
    std::mutex LFUmutex_;
    size_t put_count_;

    void evictLFU();
    

    void increase_frequency(const Key& key);
    void freqDecay();
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

    size_t    capacity_;  // 总容量
    int       sliceNum_;  // 切片数量
    std::vector<std::unique_ptr<LFUMCache<Key, Value>>> lfuSliceCaches_;

public:

   HashLFUCache(size_t capacity, int sliceNum);
   void put(const Key& key, const Value& value);
   bool get(const Key& key, Value& value);
   Value get(const Key& key);
    void purge();

};// class HashLFUCache

}  // namespace CacheDemo

#include "../src/LFUCache.tpp" 