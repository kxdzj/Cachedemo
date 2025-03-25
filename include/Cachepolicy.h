#pragma once

namespace CacheDemo
{

template<typename Key, typename Value>
class Cachepolicy
{
public:
    virtual ~Cachepolicy() = default;

    // 添加缓存接口
    virtual void put(const Key& key, const Value& value) = 0;

    // 添加查询接口,支持传出参数方式,查询成功返回true
    virtual bool get(const Key& key, Value& value) = 0;

};

} // namespace CacheDemo
