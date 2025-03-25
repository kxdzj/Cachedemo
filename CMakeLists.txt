cmake_minimum_required(VERSION 3.10)

project(CacheSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 自动查找 src/ 目录下的所有 .cpp 文件
file(GLOB CACHE_SRC "src/*.cpp")
file(GLOB CACHE_TPP "src/*.tpp")

# 输出调试信息
message(STATUS "CACHE_SRC: ${CACHE_SRC}")
message(STATUS "CACHE_TPP: ${CACHE_TPP}")

# 创建静态库，包含 .cpp 和 .tpp 文件
add_library(cache_lib STATIC ${CACHE_SRC} ${CACHE_TPP})

# 明确指定链接语言为 C++
set_target_properties(cache_lib PROPERTIES LINKER_LANGUAGE CXX)

# 指定头文件目录（包括 include 目录）
target_include_directories(cache_lib PUBLIC include)

# 设置编译选项
target_compile_options(cache_lib PRIVATE -O2)

# 链接 pthread 库
target_link_libraries(cache_lib PUBLIC pthread)

# 测试可执行程序
add_executable(test_cache test/test_cache.cpp)

# 让 test_cache 链接 cache_lib 和 pthread
target_link_libraries(test_cache PRIVATE cache_lib pthread)

# 指定 test_cache 需要的头文件
target_include_directories(test_cache PRIVATE include)
