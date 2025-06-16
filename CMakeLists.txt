cmake_minimum_required(VERSION 3.10)
project(chat_server)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread -Wno-error=pedantic -Wno-error")

# 设置库路径
set(MUDUO_LIB_DIR ${CMAKE_SOURCE_DIR}/third_party/muduo/lib)
set(MUDUO_NET ${MUDUO_LIB_DIR}/libmuduo_net.a)
set(MUDUO_BASE ${MUDUO_LIB_DIR}/libmuduo_base.a)

# 查找其他所需库
find_library(PQXX_LIBRARY pqxx REQUIRED)
find_library(PQ_LIBRARY pq REQUIRED)

# 查找POCO库
find_package(Poco REQUIRED Foundation Net NetSSL)

# 查找redis-plus-plus
find_library(REDIS_PLUS_PLUS_LIB redis++ PATHS ${CMAKE_SOURCE_DIR}/third_party/redis-plus-plus/lib)
find_library(HIREDIS_LIB hiredis)

# 查找JsonCpp
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP jsoncpp)

# 包含目录
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/third_party/muduo/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party/redis-plus-plus/include)
include_directories(${JSONCPP_INCLUDE_DIRS})

# 源文件
set(SOURCE_FILES
    src/main.cpp
    src/model/UserModel.cpp
    src/server/ChatServer.cpp
    src/service/EmailService.cpp
    src/service/VerificationCodeService.cpp
    src/service/RedisService.cpp
    src/service/MessageArchiveService.cpp
    src/server/ChatServer.chat.cpp
    src/server/ChatServer.message.cpp
)

# 生成可执行文件
add_executable(chat_server ${SOURCE_FILES})

# 链接库
target_link_libraries(chat_server
    ${MUDUO_NET}
    ${MUDUO_BASE}
    ${PQXX_LIBRARY}
    ${PQ_LIBRARY}
    Poco::Foundation
    Poco::Net
    Poco::NetSSL
    ${REDIS_PLUS_PLUS_LIB}
    ${HIREDIS_LIB}
    ${JSONCPP_LIBRARIES}
    pthread
    rt
)

# 安装目标
install(TARGETS chat_server DESTINATION bin)
