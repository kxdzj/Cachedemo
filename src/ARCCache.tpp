#include "../include/ARCCache.h"


namespace CacheDemo {


// ArcLruPart
template<typename Key, typename Value>
ArcLruPart<Key, Value>::ArcLruPart(size_t capacity, size_t transformThreshold)
        : capacity_(capacity), transformThreshold_(transformThreshold) {
        cacheMap_.reserve(capacity);
    }

template<typename Key, typename Value>
bool ArcLruPart<Key, Value>::put(Key key, Value value) {
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

template<typename Key, typename Value>
bool ArcLruPart<Key, Value>::get(Key key, Value& value, bool& shouldTransform) {
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

template<typename Key, typename Value>
bool ArcLruPart<Key, Value>::checkGhost(Key key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return ghostCache_.find(key) != ghostCache_.end();
    }

template<typename Key, typename Value>
void ArcLruPart<Key, Value>::increaseCapacity() {
        std::lock_guard<std::mutex> lock(mutex_);
        capacity_++;
    }

template<typename Key, typename Value>
bool ArcLruPart<Key, Value>::decreaseCapacity() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (capacity_ > 1) {
            capacity_--;
            return true;
        }
        return false;
    }

template<typename Key, typename Value>
void ArcLruPart<Key, Value>::moveToFront(ListIterator nodeIt) {
        cacheList_.splice(cacheList_.begin(), cacheList_, nodeIt);
    }

template<typename Key, typename Value>    
void ArcLruPart<Key, Value>::evict() {
        if (cacheList_.empty()) return;

        auto last = --cacheList_.end();
        ghostCache_.insert(last->key);
        cacheMap_.erase(last->key);
        cacheList_.erase(last);
    }



// ArcLfuPart
template<typename Key, typename Value>
ArcLfuPart<Key, Value>:: ArcLfuPart(size_t capacity, size_t transformThreshold)
        : capacity_(capacity), transformThreshold_(transformThreshold), minFreq_(1) {}


template<typename Key, typename Value>
bool ArcLfuPart<Key, Value>:: put(Key key, Value value) {
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

template<typename Key, typename Value>
bool ArcLfuPart<Key, Value>:: get(Key key, Value& value) {
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

template<typename Key, typename Value>
bool ArcLfuPart<Key, Value>:: checkGhost(Key key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return ghostCache_.find(key) != ghostCache_.end();
}

template<typename Key, typename Value>
void ArcLfuPart<Key, Value>:: increaseCapacity() {
        std::lock_guard<std::mutex> lock(mutex_);
        capacity_++;
}

template<typename Key, typename Value>
bool ArcLfuPart<Key, Value>::decreaseCapacity() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (capacity_ > 1) {
            capacity_--;
            return true;
        }
        return false;
    }

template<typename Key, typename Value>
void ArcLfuPart<Key, Value>::increaseFreq(ListIterator nodeIt) {
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

template<typename Key, typename Value>
void ArcLfuPart<Key, Value>:: evict() {
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



// ArcCache
template<typename Key, typename Value>
ArcCache<Key, Value>:: ArcCache(size_t capacity, size_t transformThreshold)
        : capacity_(capacity), transformThreshold_(transformThreshold),
        lruPart_(std::make_unique<ArcLruPart<Key, Value>>(capacity, transformThreshold)),
        lfuPart_(std::make_unique<ArcLfuPart<Key, Value>>(capacity, transformThreshold))
    {}

template<typename Key, typename Value>
ArcCache<Key, Value>:: ~ArcCache()  = default;

template<typename Key, typename Value>
void ArcCache<Key, Value>:: put(const Key& key, const Value& value)  {
        bool inGhost = checkGhostCaches(key);
 
        if (!inGhost) {
            if (lruPart_->put(key, value)) {
                lfuPart_->put(key, value);
            }
        } else {
            lruPart_->put(key, value);
        }
       
    }

template<typename Key, typename Value>
bool ArcCache<Key, Value>:: get(const Key& key, Value& value)  {
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

template<typename Key, typename Value>
Value ArcCache<Key, Value>:: get(Key key)  {
        Value value{};
        get(key, value);
        return value;
}

template<typename Key, typename Value>
bool ArcCache<Key, Value>:: checkGhostCaches(Key key) {
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

} // namespace CacheDemo
