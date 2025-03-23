#include <iostream>
#include <chrono>
#include <thread>
#include<atomic>
#include <vector>
#include <iomanip>
#include <random>
#include <algorithm>

#include "../src/FIFOCache.h"
#include "../src/LRUCache.h"
#include "../src/LFUCache.h"
#include "../src/ARCCache.h"

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
    std::cout << "LFU - 命中率: " << std::fixed << std::setprecision(2) 
            << (100.0 * hits[3] / get_operations[3]) << "%" << std::endl;
    std::cout << "LFUM - 命中率: " << std::fixed << std::setprecision(2) 
            << (100.0 * hits[4] / get_operations[4]) << "%" << std::endl;
    std::cout << "ARC - 命中率: " << std::fixed << std::setprecision(2) 
            << (100.0 * hits[5] / get_operations[5]) << "%" << std::endl;
}



void print2Results(const std::string& testName, int capacity, 
    const std::vector<int>& get_operations, 
    const std::vector<int>& hits) {
    std::cout << "测试场景: " << testName << std::endl;
    std::cout << "缓存大小: " << capacity << std::endl;
    std::cout << "LRU - 命中率: " << std::fixed << std::setprecision(2) 
            << (100.0 * hits[0] / get_operations[0]) << "%" << std::endl;
    std::cout << "LFU - 命中率: " << std::fixed << std::setprecision(2) 
            << (100.0 * hits[1] / get_operations[1]) << "%" << std::endl;
    std::cout << "ARC - 命中率: " << std::fixed << std::setprecision(2) 
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
    std::cout << "\n=== 测试多线程普通LRU缓存性能 ===" << std::endl;

    const int CAPACITY = 500;
    const int OPERATIONS = 10000000;
    const int HOT_KEYS = 100;
    const int COLD_KEYS = 10000;

    const int threadnum = 4;
    if( OPERATIONS % threadnum ){
        std::cout<<"\n操作数不可以整除线程数"<<std::endl;
    }

    // 创建 LRU 缓存
    // CacheDemo::HashLRUCache<int, std::string> lru_cache(CAPACITY, threadnum);
    CacheDemo::LRUCache<int, std::string> lru_cache(CAPACITY);

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

void test_hashmulti_performance() {
    std::cout << "\n=== 测试多线程HASHLRU缓存性能 ===" << std::endl;

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
    std::cout << "HASHLRU - 命中率: " << std::fixed << std::setprecision(2) << hit_rate << "%\n";
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
    CacheDemo::LFUCache<int, std::string> lfu(CAPACITY);
    CacheDemo::LFUMCache<int, std::string> lfum(CAPACITY);
    CacheDemo::ArcCache<int, std::string> arc(CAPACITY);

    std::random_device rd;
    std::mt19937 gen(rd());

    std::array<CacheDemo::Cachepolicy<int, std::string>*, 6> caches = {&fifo, &lru, &lruk, &lfu, &lfum, &arc};
    std::vector<int> hits(6, 0);
    std::vector<int> get_operations(6, 0);

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



    
    
void testLoopPattern() {
        std::cout << "\n=== 测试场景2：循环扫描测试 ===" << std::endl;
        
        const int CAPACITY = 50;  // 增加缓存容量
        const int LOOP_SIZE = 500;         
        const int OPERATIONS = 200000;  // 增加操作次数
        
        CacheDemo::LRUCache<int, std::string> lru(CAPACITY);
        CacheDemo::LFUMCache<int, std::string> lfu(CAPACITY);
        CacheDemo::ArcCache<int, std::string> arc(CAPACITY);
    
        std::array<CacheDemo::Cachepolicy<int, std::string>*, 3> caches = {&lru, &lfu, &arc};
        std::vector<int> hits(3, 0);
        std::vector<int> get_operations(3, 0);
    
        std::random_device rd;
        std::mt19937 gen(rd());
    
        // 先填充数据
        for (int i = 0; i < caches.size(); ++i) {
            for (int key = 0; key < LOOP_SIZE; ++key) {  // 只填充 LOOP_SIZE 的数据
                std::string value = "loop" + std::to_string(key);
                caches[i]->put(key, value);
            }
            
            // 然后进行访问测试
            int current_pos = 0;
            for (int op = 0; op < OPERATIONS; ++op) {
                int key;
                if (op % 100 < 60) {  // 60%顺序扫描
                    key = current_pos;
                    current_pos = (current_pos + 1) % LOOP_SIZE;
                } else if (op % 100 < 90) {  // 30%随机跳跃
                    key = gen() % LOOP_SIZE;
                } else {  // 10%访问范围外数据
                    key = LOOP_SIZE + (gen() % LOOP_SIZE);
                }
                
                std::string result;
                get_operations[i]++;
                if (caches[i]->get(key, result)) {
                    hits[i]++;
                }
            }
        }
    
        print2Results("循环扫描测试", CAPACITY, get_operations, hits);
    }
    
void testWorkloadShift() {
        std::cout << "\n=== 测试场景3：工作负载剧烈变化测试 ===" << std::endl;
        
        const int CAPACITY = 4;            
        const int OPERATIONS = 80000;      
        const int PHASE_LENGTH = OPERATIONS / 5;
        
        CacheDemo::LRUCache<int, std::string> lru(CAPACITY);
        CacheDemo::LFUCache<int, std::string> lfu(CAPACITY);
        CacheDemo::ArcCache<int, std::string> arc(CAPACITY);
    
        std::random_device rd;
        std::mt19937 gen(rd());
        std::array<CacheDemo::Cachepolicy<int, std::string>*, 3> caches = {&lru, &lfu, &arc};
        std::vector<int> hits(3, 0);
        std::vector<int> get_operations(3, 0);
    
        // 先填充一些初始数据
        for (int i = 0; i < caches.size(); ++i) {
            for (int key = 0; key < 1000; ++key) {
                std::string value = "init" + std::to_string(key);
                caches[i]->put(key, value);
            }
            
            // 然后进行多阶段测试
            for (int op = 0; op < OPERATIONS; ++op) {
                int key;
                // 根据不同阶段选择不同的访问模式
                if (op < PHASE_LENGTH) {  // 热点访问
                    key = gen() % 5;
                } else if (op < PHASE_LENGTH * 2) {  // 大范围随机
                    key = gen() % 1000;
                } else if (op < PHASE_LENGTH * 3) {  // 顺序扫描
                    key = (op - PHASE_LENGTH * 2) % 100;
                } else if (op < PHASE_LENGTH * 4) {  // 局部性随机
                    int locality = (op / 1000) % 10;
                    key = locality * 20 + (gen() % 20);
                } else {  // 混合访问
                    int r = gen() % 100;
                    if (r < 30) {
                        key = gen() % 5;
                    } else if (r < 60) {
                        key = 5 + (gen() % 95);
                    } else {
                        key = 100 + (gen() % 900);
                    }
                }
                
                std::string result;
                get_operations[i]++;
                if (caches[i]->get(key, result)) {
                    hits[i]++;
                }
                
                // 随机进行put操作，更新缓存内容
                if (gen() % 100 < 30) {  // 30%概率进行put
                    std::string value = "new" + std::to_string(key);
                    caches[i]->put(key, value);
                }
            }
        }
        print2Results("工作负载剧烈变化测试", CAPACITY, get_operations, hits);
}
    

// **主函数**
int main()
{
    // test_multithreading();
    testHotDataAccess();
    benchmark("单线程LRU测试开始：", test_single_performance);
    benchmark("多线程LRU测试开始：", test_multi_performance);
    benchmark("多线程分片LRU测试开始：", test_hashmulti_performance);
    benchmark("循环扫描测试开始：", testLoopPattern);
    benchmark("剧烈变动工作环境开始：", testWorkloadShift);
    return 0;
}
