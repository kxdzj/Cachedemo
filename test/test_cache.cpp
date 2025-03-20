#include <iostream>
#include <chrono>
#include <thread>
#include<atomic>
#include <vector>
#include <iomanip>
#include <random>
#include <algorithm>

#include "../src/LRUCache.h"
#include "../src/FIFOCache.h"

using namespace CacheDemo;

// 性能测试工具函数
template <typename Func>
void benchmark(const std::string& test_name, Func func)
{
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << test_name << " 耗时 " << duration.count() << " ms\n";
}

// 辅助函数：打印命中率结果
void printResults(const std::string& testName, int capacity, 
    const std::vector<int>& get_operations, 
    const std::vector<int>& hits) {
    std::cout << "测试场景: " << testName << std::endl;
    std::cout << "缓存大小: " << capacity << std::endl;
    std::cout << "FIFO - 命中率: " << std::fixed << std::setprecision(2) 
            << (100.0 * hits[0] / get_operations[0]) << "%" << std::endl;
    std::cout << "LRU - 命中率: " << std::fixed << std::setprecision(2) 
            << (100.0 * hits[1] / get_operations[1]) << "%" << std::endl;
    std::cout << "LRUK - 命中率: " << std::fixed << std::setprecision(2) 
            << (100.0 * hits[2] / get_operations[2]) << "%" << std::endl;
}

// **多线程测试**
void test_multithreading()
{
    std::cout << "\n=== 测试多线程缓存安全性 ===" << std::endl;
    const int CAPACITY = 50;
    CacheDemo::LRUCache<int, int> cache(CAPACITY);

    auto writer = [&cache]() {
        for (int i = 0; i < 5000; i++)
        {
            cache.put(i % 50, i);
        }
    };

    auto reader = [&cache]() {
        int value;
        for (int i = 0; i < 5000; i++)
        {
            cache.get(i % 50, value);
        }
    };

    // **创建多个线程进行并发读写**
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) { 
        threads.emplace_back(writer);
        threads.emplace_back(reader);
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "多线程测试完成\n";
}

// **缓存性能测试**
void test_single_performance()
{
    std::cout << "\n=== 测试单线程缓存性能 ===" << std::endl;

    const int CAPACITY = 500;
    const int HISTORYCAP = 500;
    const int OPERATIONS = 10000000;
    const int HOT_KEYS = 100;
    const int COLD_KEYS = 10000;


    CacheDemo::HashLRUCache<int, std::string> lru_cache(CAPACITY, 4);
    // CacheDemo::FIFOCache<int, std::string> fifo_cache(CAPACITY);
    // CacheDemo::LRUKCache<int, std::string> lruk_cache();

    std::random_device rd;
    std::mt19937 gen(rd());


    for (int op = 0; op < OPERATIONS; ++op) {
        int key;
        if (op % 100 < 70) {  // 70%热点数据
            key = gen() % HOT_KEYS;
        } else {  // 30%冷数据
            key = HOT_KEYS + (gen() % COLD_KEYS);
        }
        std::string value = "value" + std::to_string(key);
        lru_cache.put(key, value);
    }

    int hit = 0;
    for (int get_op = 0; get_op < OPERATIONS; ++get_op) {
        int key;
        if (get_op % 100 < 70) {  // 70%访问热点
            key = gen() % HOT_KEYS;
        } else {  
            key = HOT_KEYS + (gen() % COLD_KEYS);
        }
        std::string result;
        if (lru_cache.get(key, result)) {
            ++hit;
        }
    }

    std::cout << "LRU - 命中率: " << std::fixed << std::setprecision(2) 
            << (100.0 * hit / OPERATIONS) << "%" << std::endl;
    
}

void test_multi_performance() {
    std::cout << "\n=== 测试多线程缓存性能 ===" << std::endl;

    const int CAPACITY = 500;
    const int OPERATIONS = 10000000;
    const int HOT_KEYS = 100;
    const int COLD_KEYS = 10000;

    const int threadnum = 4;
    if( OPERATIONS % threadnum ){
        std::cout<<"\n操作数不可以整除线程数"<<std::endl;
    }

    // 创建 LRU 缓存
    CacheDemo::HashLRUCache<int, std::string> lru_cache(CAPACITY, threadnum);
    // CacheDemo::LRUCache<int, std::string> lru_cache(CAPACITY);

    // 统计命中次数
    std::atomic<int> hit_count(0);

    // 线程任务：插入数据
    auto put_task = [&]() {
        std::random_device rd;
        std::mt19937 gen(rd());

        for (int i = 0; i < OPERATIONS / threadnum; ++i) {  // 每个线程执行一半
            int key = (i % 100 < 70) ? gen() % HOT_KEYS : HOT_KEYS + (gen() % COLD_KEYS);
            std::string value = "value" + std::to_string(key);
            lru_cache.put(key, value);
        }
        
    };

    // 线程任务：查询数据
    auto get_task = [&]() {
        std::random_device rd;
        std::mt19937 gen(rd());

        for (int i = 0; i < OPERATIONS / threadnum; ++i) {  // 每个线程执行一半
            int key = (i % 100 < 70) ? gen() % HOT_KEYS : HOT_KEYS + (gen() % COLD_KEYS);
            std::string result;
            if (lru_cache.get(key, result)) {
                hit_count++;  // 线程安全统计命中次数
            }
        }
    };


    // 启动 4 个线程（2 个插入 + 2 个查询）
    std::vector<std::thread> threads;

    // 先启动写入线程
    for (int i = 0; i < threadnum; ++i) {
        threads.emplace_back(put_task);
    }

    // 等待写入线程执行完毕
    for (auto& t : threads) {
        t.join();
    }

    // 清空线程 vector
    threads.clear();

    // 再启动查询线程
    for (int i = 0; i < threadnum; ++i) {
        threads.emplace_back(get_task);
    }

    // 等待查询线程执行完毕
    for (auto& t : threads) {
        t.join();
    }

    // 清空线程 vector
    threads.clear();

    // 计算命中率
    double hit_rate = (100.0 * hit_count.load() / OPERATIONS);
    std::cout << "LRU - 命中率: " << std::fixed << std::setprecision(2) << hit_rate << "%\n";
}


// **热点数据访问测试**
void testHotDataAccess() {
    std::cout << "\n=== 测试热点数据访问 ===" << std::endl;

    const int CAPACITY = 50;  
    const int HISCAPACITY = 50; 
    const int OPERATIONS = 1000000;  
    const int HOT_KEYS = 20;  
    const int COLD_KEYS = 5000;      

    CacheDemo::FIFOCache<int, std::string> fifo(CAPACITY);
    CacheDemo::LRUCache<int, std::string> lru(CAPACITY);
    CacheDemo::LRUKCache<int, std::string> lruk(CAPACITY, HISCAPACITY, 2);

    std::random_device rd;
    std::mt19937 gen(rd());

    std::array<CacheDemo::Cachepolicy<int, std::string>*, 3> caches = {&fifo, &lru, &lruk};
    std::vector<int> hits(3, 0);
    std::vector<int> get_operations(3, 0);

    for (int i = 0; i < caches.size(); ++i) {
        for (int op = 0; op < OPERATIONS; ++op) {
            int key;
            if (op % 100 < 70) {  // 70%热点数据
                key = gen() % HOT_KEYS;
            } else {  // 30%冷数据
                key = HOT_KEYS + (gen() % COLD_KEYS);
            }
            std::string value = "value" + std::to_string(key);
            caches[i]->put(key, value);
        }

        for (int get_op = 0; get_op < OPERATIONS; ++get_op) {
            int key;
            if (get_op % 100 < 70) {  // 70%访问热点
                key = gen() % HOT_KEYS;
            } else {  
                key = HOT_KEYS + (gen() % COLD_KEYS);
            }
            std::string result;
            get_operations[i]++;
            if (caches[i]->get(key, result)) {
                hits[i]++;
            }
        }
    }

    printResults("热点数据访问", CAPACITY, get_operations, hits);
}

// **主函数**
int main()
{
    test_multithreading();
    testHotDataAccess();
    benchmark("单线程测试开始：", test_single_performance);
    benchmark("多线程线程测试开始：", test_multi_performance);
    return 0;
}
