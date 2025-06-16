#!/bin/bash

# 创建构建目录
mkdir -p smtp_test_cmake/build
cd smtp_test_cmake/build

# 配置和构建
cmake ../
make

echo "编译完成。运行: ./smtp_cpp_test [收件人邮箱]"
