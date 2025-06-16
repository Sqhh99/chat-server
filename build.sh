#!/bin/bash

# 输出颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}===== 构建聊天服务器 =====${NC}"

# 创建构建目录（如果不存在）
if [ ! -d "build" ]; then
    echo -e "${YELLOW}Creating build directory...${NC}"
    mkdir -p build
fi

# 进入构建目录
cd build

# 运行CMake生成Makefile
echo -e "${YELLOW}Running CMake...${NC}"
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 构建项目
echo -e "${YELLOW}Building project...${NC}"
make -j$(nproc)

# 检查构建结果
if [ $? -eq 0 ]; then
    echo -e "${GREEN}Build successful!${NC}"
    echo -e "${GREEN}Executable location: $(pwd)/chat_server${NC}"
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

# 返回到项目根目录
cd ..

echo -e "${GREEN}===== 构建完成 =====${NC}"
