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

    explicit ArcLruPart(size_t capacity, size_t transformThreshold)
        : capacity_(capacity), transformThreshold_(transformThreshold) {
        cacheMap_.reserve(capacity);
    }

    bool put(Key key, Value value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cacheMap_.find(key);
        if (it != cacheMap_.end()) {
            // 更新值并增加访问次数
            it->second->value = value;
            it->second->freq++;
            moveToFront(it->second);
            return false;
        }

        if (cacheList_.size() >= capacity_) {
            evict();
        }

        // 创建新节点并将其插入到链表头
        cacheList_.push_front(Node<Key, Value>(key, value));
        cacheMap_[key] = cacheList_.begin();  // 保存节点的迭代器
        return true;
    }

    bool get(Key key, Value& value, bool& shouldTransform) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cacheMap_.find(key);
        if (it == cacheMap_.end()) {
            return false;
        }

        value = it->second->value;
        it->second->freq++;  // 增加访问次数
        moveToFront(it->second);

        if (it->second->freq >= transformThreshold_) {
            shouldTransform = true;  // 触发迁移到 LFU
        }

        return true;
    }

    bool checkGhost(Key key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return ghostCache_.find(key) != ghostCache_.end();
    }

    void increaseCapacity() {
        std::lock_guard<std::mutex> lock(mutex_);
        capacity_++;
    }

    bool decreaseCapacity() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (capacity_ > 1) {
            capacity_--;
            return true;
        }
        return false;
    }

private:
    void moveToFront(ListIterator nodeIt) {
        cacheList_.splice(cacheList_.begin(), cacheList_, nodeIt);
    }

    void evict() {
        if (cacheList_.empty()) return;

        auto last = --cacheList_.end();
        ghostCache_.insert(last->key);
        cacheMap_.erase(last->key);
        cacheList_.erase(last);
    }

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
    
    explicit ArcLfuPart(size_t capacity, size_t transformThreshold)
        : capacity_(capacity), transformThreshold_(transformThreshold), minFreq_(1) {}

    bool put(Key key, Value value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cacheMap_.find(key);
        if (it != cacheMap_.end()) {
            // 更新值并增加访问次数
            ListIterator nodeIt = it->second;
            nodeIt->value = value;
            increaseFreq(nodeIt);  // 更新频率并将节点移到正确的频率链表
            return true;
        }

        if (cacheMap_.size() >= capacity_) {
            evict();
        }

        // 插入新节点，频率为 1
        freqMap_[1].emplace_front(key, value, 1);
        cacheMap_[key] = freqMap_[1].begin();  // 存储指向新插入节点的迭代器
        minFreq_ = 1;  // 初始时最低频率为 1
        return true;
    }

    bool get(Key key, Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cacheMap_.find(key);
        if (it == cacheMap_.end()) {
            return false;
        }

        ListIterator nodeIt = it->second;
        value = nodeIt->value;
        increaseFreq(nodeIt);  // 增加频率并移动节点
        return true;
    }

    bool checkGhost(Key key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return ghostCache_.find(key) != ghostCache_.end();
    }

    void increaseCapacity() {
        std::lock_guard<std::mutex> lock(mutex_);
        capacity_++;
    }

    bool decreaseCapacity() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (capacity_ > 1) {
            capacity_--;
            return true;
        }
        return false;
    }

private:
    void increaseFreq(ListIterator nodeIt) {
        size_t oldFreq = nodeIt->freq;
        size_t newFreq = oldFreq + 1;

        // 先在 cacheMap_ 里删除旧的迭代器
        cacheMap_.erase(nodeIt->key);

        // 先移除旧频次链表中的节点
        freqMap_[oldFreq].erase(nodeIt);
        if (freqMap_[oldFreq].empty()) {
            freqMap_.erase(oldFreq);
        }

        // 插入到新频次的链表头部
        freqMap_[newFreq].emplace_front(nodeIt->key, nodeIt->value, newFreq);
        cacheMap_[nodeIt->key] = freqMap_[newFreq].begin();

        // 更新 minFreq_
        if (freqMap_.find(minFreq_) == freqMap_.end()) {
            minFreq_ = newFreq;
        }
    }

    void evict() {
        if (freqMap_.empty()) return;

        // 淘汰最低频率的节点
        auto& minFreqList = freqMap_[minFreq_];
        auto last = --minFreqList.end();
        ghostCache_.insert(last->key);  // 将淘汰的节点放入 ghostCache
        cacheMap_.erase(last->key);
        minFreqList.erase(last);

        // 如果当前最低频率的链表为空，更新最低频率
        if (minFreqList.empty()) {
            freqMap_.erase(minFreq_);
            // 遍历 freqMap_ 找到新的最小频率
            if (!freqMap_.empty()) {
                minFreq_ = freqMap_.begin()->first;
            } else {
                minFreq_ = 1;  // 如果 freqMap_ 为空，重置 minFreq_
            }
        }
    }

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
    explicit ArcCache(size_t capacity, size_t transformThreshold = 2)
        : capacity_(capacity), transformThreshold_(transformThreshold),
        lruPart_(std::make_unique<ArcLruPart<Key, Value>>(capacity, transformThreshold)),
        lfuPart_(std::make_unique<ArcLfuPart<Key, Value>>(capacity, transformThreshold))
    {}

    ~ArcCache() override = default;

    void put(const Key& key, const Value& value) override {
        bool inGhost = checkGhostCaches(key);
 
        if (!inGhost) {
            if (lruPart_->put(key, value)) {
                lfuPart_->put(key, value);
            }
        } else {
            lruPart_->put(key, value);
        }
       
    }

    bool get(const Key& key, Value& value) override {
        checkGhostCaches(key);
      
        bool shouldTransform = false;
        if (lruPart_->get(key, value, shouldTransform)) {
            if (shouldTransform) {
                lfuPart_->put(key, value);
            }
            return true;
        }
       
        return lfuPart_->get(key, value);
    }

    Value get(Key key)  {
        Value value{};
        get(key, value);
        return value;
    }

private:
    bool checkGhostCaches(Key key) {
        bool inGhost = false;
        if (lruPart_->checkGhost(key)) {
            if (lfuPart_->decreaseCapacity()) {
                lruPart_->increaseCapacity();
            }
            inGhost = true;
        }
        else if (lfuPart_->checkGhost(key)) {
            if (lruPart_->decreaseCapacity()) {
                lfuPart_->increaseCapacity();
            }
            inGhost = true;
        }
        return inGhost;
    }

private:
    size_t capacity_;
    size_t transformThreshold_;
    std::unique_ptr<ArcLruPart<Key, Value>> lruPart_;
    std::unique_ptr<ArcLfuPart<Key, Value>> lfuPart_;
};

} // namespace CacheDemo
