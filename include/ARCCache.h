#pragma once

#include <cmath>
#include <list>
#include <vector>
#include <memory>
#include <iostream>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include "Cachepolicy.h"
using namespace std;
namespace CacheDemo {

// Node
template<typename Key, typename Value>
struct Node {
  Key key;
  Value value;
  size_t freq;
  typename std::list<Node>::iterator listIt;  // 记录该节点在链表中的位置

  Node(Key k, Value v, size_t f = 1)
      : key(k), value(v), freq(f) {}
};

// ArcLruPart
template<typename Key, typename Value>
class ArcLruPart {
public:
    using ListType = std::list<Node<Key, Value>>;  // 使用 Node<Key, Value>
    using ListIterator = typename ListType::iterator;
    using Hashmap = std::unordered_map<Key, ListIterator>;  // {key, ListIterator}

    explicit ArcLruPart(size_t capacity, size_t transformThreshold);

    bool put(Key key, Value value);
    bool get(Key key, Value& value, bool& shouldTransform);
    bool checkGhost(Key key);
    void increaseCapacity();
    bool decreaseCapacity();

private:
    void moveToFront(ListIterator nodeIt); 
    void evict(); 

private:
    size_t capacity_;
    size_t transformThreshold_;
    ListType cacheList_;   // 维护 LRU 访问顺序{list<node>}
    Hashmap cacheMap_;  // {key, list<node>->iterator}
    std::unordered_set<Key> ghostCache_;  // 存储淘汰的 key
    std::mutex mutex_;  // 用于加锁
};

// ArcLfuPart
template<typename Key, typename Value>
class ArcLfuPart {
public:
    using ListType = std::list<Node<Key, Value>>;  // 使用 Node<Key, Value>
    using ListIterator = typename ListType::iterator;
    using Hashmap = std::unordered_map<Key, ListIterator>;  // {key, Node iterator}
    using FreqMap = std::unordered_map<size_t, ListType>;  // freq -> ListType
    
    explicit ArcLfuPart(size_t capacity, size_t transformThreshold);

    bool put(Key key, Value value);
    bool get(Key key, Value& value); 
    bool checkGhost(Key key);
    void increaseCapacity();
    bool decreaseCapacity(); 

private:
    void increaseFreq(ListIterator nodeIt);
    void evict();

private:
    size_t capacity_;
    size_t transformThreshold_;
    size_t minFreq_;  // 记录当前最低频率
    FreqMap freqMap_;  // 存储频率到节点链表的映射
    Hashmap cacheMap_; // 存储 key 到节点迭代器的映射
    std::unordered_set<Key> ghostCache_;  // 存储被淘汰的 key
    std::mutex mutex_;  // 用于加锁
};


// ArcCache
template<typename Key, typename Value>
class ArcCache : public Cachepolicy<Key, Value> {
public:
    explicit ArcCache(size_t capacity, size_t transformThreshold = 2);
    ~ArcCache() override;

    void put(const Key& key, const Value& value) override;

    bool get(const Key& key, Value& value) override;
    Value get(Key key);

private:
    bool checkGhostCaches(Key key);

private:
    size_t capacity_;
    size_t transformThreshold_;
    std::unique_ptr<ArcLruPart<Key, Value>> lruPart_;
    std::unique_ptr<ArcLfuPart<Key, Value>> lfuPart_;
};

} // namespace CacheDemo
#include "../src/ARCCache.tpp" 