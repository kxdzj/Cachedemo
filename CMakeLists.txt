# 设置最低 CMake 版本要求
cmake_minimum_required(VERSION 3.10)

project(CacheSystem)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
# 开启优化和多线程支持
set(CMAKE_CXX_FLAGS "-O2 -pthread")  


# 让 CMake 知道 src 目录的头文件
include_directories(src)

# 添加测试代码
add_executable(test_cache test/test_cache.cpp)
