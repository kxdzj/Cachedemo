#include "../include/LRUCache.h"

namespace CacheDemo {

template<typename Key, typename Value>
LRUCache<Key, Value>::LRUCache(size_t cap) : capacity_(cap)
{
    Cachemap_.reserve(cap);
}

template<typename Key, typename Value>
LRUCache<Key, Value>::~LRUCache() = default;

template<typename Key, typename Value>
void LRUCache<Key, Value>::put(const Key& key, const Value& value)
{
    if (capacity_ == 0) return;

    std::lock_guard<std::mutex> lock(LRUmutex_);

    auto it = Cachemap_.find(key);
    if (it != Cachemap_.end()) {
        Cachelist_.erase(it->second);
        Cachemap_.erase(it);
    }

    if (Cachelist_.size() >= capacity_) {
        auto tail = Cachelist_.back();
        Cachemap_.erase(tail.first);
        Cachelist_.pop_back();
    }

    Cachelist_.push_front({key, value});
    Cachemap_[key] = Cachelist_.begin();
}

template<typename Key, typename Value>
bool LRUCache<Key, Value>::get(const Key& key, Value& value)
{
    std::lock_guard<std::mutex> lock(LRUmutex_);

    auto it = Cachemap_.find(key);
    if (it == Cachemap_.end()) {
        return false;
    }

    value = it->second->second;
    Cachelist_.splice(Cachelist_.begin(), Cachelist_, it->second);

    return true;
}

template<typename Key, typename Value>
void LRUCache<Key, Value>::deletenode(const Key& key)
{
    std::lock_guard<std::mutex> lock(LRUmutex_);

    auto it = Cachemap_.find(key);
    if (it == Cachemap_.end()) {
        return;
    }

    Cachelist_.erase(it->second);
    Cachemap_.erase(it);
}

// LRUKCache 实现

template<typename Key, typename Value>
LRUKCache<Key, Value>::LRUKCache(size_t cap, size_t historycap, int k)
    : LRUCache<Key, Value>(cap),
      historylist_(std::make_unique<LRUCache<Key, size_t>>(historycap)),
      k_(k) {}

template<typename Key, typename Value>
void LRUKCache<Key, Value>::put(const Key& key, const Value& value)
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

template<typename Key, typename Value>
bool LRUKCache<Key, Value>::get(const Key& key, Value& value)
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

// HashLRUCache 实现

template<typename Key, typename Value>
size_t HashLRUCache<Key, Value>::Hash(Key key)
{
    std::hash<Key> hashFunc;
    return hashFunc(key);
}

template<typename Key, typename Value>
HashLRUCache<Key, Value>::HashLRUCache(size_t capacity, int slicenum)
    : capacity_(capacity),
      sliceNum_(slicenum > 0 ? slicenum : std::thread::hardware_concurrency())
{
    size_t sliceSize = std::ceil(capacity / static_cast<double>(sliceNum_));
    for (int i = 0; i < sliceNum_; ++i) {
        lruSliceCaches_.emplace_back(std::make_unique<LRUCache<Key, Value>>(sliceSize));
    }
}

template<typename Key, typename Value>
void HashLRUCache<Key, Value>::put(const Key& key, const Value& value)
{
    size_t sliceIndex = Hash(key) % sliceNum_;
    lruSliceCaches_[sliceIndex]->put(key, value);
}

template<typename Key, typename Value>
bool HashLRUCache<Key, Value>::get(const Key& key, Value& value)
{
    size_t sliceIndex = Hash(key) % sliceNum_;
    return lruSliceCaches_[sliceIndex]->get(key, value);
}

template<typename Key, typename Value>
Value HashLRUCache<Key, Value>::get(const Key& key)
{
    Value value{};
    get(key, value);
    return value;
}

} // namespace CacheDemo
