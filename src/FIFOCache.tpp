#include"../include/FIFOCache.h"

namespace CacheDemo
{
    
template<typename Key, typename Value>
FIFOCache<Key, Value>::FIFOCache(size_t cap):capacity_(cap) 
    {
        Cachemap_.reserve(cap); 
    }

template<typename Key, typename Value>   
FIFOCache<Key, Value>::~FIFOCache() = default;

template<typename Key, typename Value>
void FIFOCache<Key, Value>::put(const Key& key, const Value& value) 
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
            Cachemap_.erase(tail.first);
            Cachelist_.pop_back();
        }

        Cachelist_.push_front({key, value});
        Cachemap_[key] = Cachelist_.begin();
    }

template<typename Key, typename Value>
bool FIFOCache<Key, Value>::get(const Key& key, Value& value) 
    {
        std::lock_guard<std::mutex> lock(fifomutex_);

        auto it = Cachemap_.find(key);
        if(it == Cachemap_.end()){
            return false;
        }

        value = it->second->second;
        return true;

    }

template<typename Key, typename Value>
void FIFOCache<Key, Value>::deletenode(const Key& key){
        std::lock_guard<std::mutex> lock(fifomutex_);

        auto it = Cachemap_.find(key);
        if(it == Cachemap_.end()){
            return;
        }

        Cachelist_.erase(it->second);
        Cachemap_.erase(it);

    }

} // namespace CacheDemo