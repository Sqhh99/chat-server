#!/bin/bash

# 数据库配置
DB_NAME="chatserver"
DB_USER="postgres"
DB_PASSWORD="postgres"

# 检查PostgreSQL服务是否运行
echo "检查PostgreSQL服务..."
if ! pg_isready > /dev/null 2>&1; then
    echo "错误: PostgreSQL服务未运行"
    exit 1
fi

# 检查数据库是否存在，如果不存在则创建
echo "检查数据库是否存在..."
if ! psql -lqt | cut -d \| -f 1 | grep -qw $DB_NAME; then
    echo "创建数据库: $DB_NAME"
    createdb -U $DB_USER $DB_NAME
    if [ $? -ne 0 ]; then
        echo "错误: 无法创建数据库"
        exit 1
    fi
else
    echo "数据库 $DB_NAME 已存在"
fi

# 运行SQL脚本创建表
echo "创建用户表..."
psql -U $DB_USER -d $DB_NAME -f ./database/user_schema.sql

echo "创建消息表..."
psql -U $DB_USER -d $DB_NAME -f ./database/messages_schema.sql

echo "数据库初始化完成！"
