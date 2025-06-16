cmake_minimum_required(VERSION 3.10)
project(smtp_cpp_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找OpenSSL
find_package(OpenSSL REQUIRED)

# 添加可执行文件
add_executable(smtp_cpp_test smtp_cpp_test.cpp)

# 设置包含目录
include_directories(/usr/include)

# 设置库目录
link_directories(/usr/lib/x86_64-linux-gnu)

# 手动链接POCO库
target_link_libraries(smtp_cpp_test 
    PRIVATE
    PocoNetSSL
    PocoNet
    PocoFoundation
    PocoUtil
    OpenSSL::SSL
    OpenSSL::Crypto
)
