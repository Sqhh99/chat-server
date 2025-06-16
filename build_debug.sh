#!/bin/bash

# 输出颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}===== 构建并调试聊天服务器 =====${NC}"

# 创建构建目录（如果不存在）
if [ ! -d "build" ]; then
    echo -e "${YELLOW}Creating build directory...${NC}"
    mkdir -p build
fi

# 进入构建目录
cd build

# 运行CMake生成Makefile
echo -e "${YELLOW}Running CMake...${NC}"
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# 构建项目并输出详细信息
echo -e "${YELLOW}Building project with verbose output...${NC}"
make VERBOSE=1

# 检查构建结果
if [ $? -eq 0 ]; then
    echo -e "${GREEN}Build successful!${NC}"
    echo -e "${GREEN}Executable location: $(pwd)/chat_server${NC}"
else
    echo -e "${RED}Build failed!${NC}"
    echo -e "${YELLOW}Checking compilation issues with specific files...${NC}"
    
    echo -e "${YELLOW}Testing compilation of RedisService.cpp...${NC}"
    g++ -c ../src/service/RedisService.cpp -I.. -I../third_party/muduo/include -I../third_party/redis-plus-plus/include -I/usr/include/jsoncpp -std=c++17
    
    echo -e "${YELLOW}Testing compilation of ChatServer.cpp...${NC}"
    g++ -c ../src/server/ChatServer.cpp -I.. -I../third_party/muduo/include -I../third_party/redis-plus-plus/include -I/usr/include/jsoncpp -std=c++17
    
    echo -e "${YELLOW}Testing compilation of ChatServer.chat.cpp...${NC}"
    g++ -c ../src/server/ChatServer.chat.cpp -I.. -I../third_party/muduo/include -I../third_party/redis-plus-plus/include -I/usr/include/jsoncpp -std=c++17
    
    echo -e "${RED}Build failed with detailed diagnostic information above.${NC}"
    exit 1
fi

# 返回到项目根目录
cd ..

echo -e "${GREEN}===== 构建完成 =====${NC}"
