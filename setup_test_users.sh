#!/bin/bash

# PostgreSQL数据库连接配置
# 请根据您的实际Docker配置修改这些变量
DB_HOST="localhost"  # 如果是Docker，可能需要使用容器名或IP
DB_PORT="5432"
DB_NAME="chat_server"  # 或者 chatserver，根据您的数据库名
DB_USER="sqhh99"      # 或者 postgres
DB_PASSWORD="your_password"

echo "正在创建测试用户..."

# 方法1: 如果您有本地psql客户端
# PGPASSWORD=$DB_PASSWORD psql -h $DB_HOST -p $DB_PORT -U $DB_USER -d $DB_NAME -f create_test_users.sql

# 方法2: 如果使用Docker（请替换container_name为您的实际容器名）
# docker exec -i your_postgres_container_name psql -U $DB_USER -d $DB_NAME < create_test_users.sql

# 方法3: 直接执行SQL命令
cat << 'EOF'
您可以手动执行以下SQL命令：

-- 创建测试用户
INSERT INTO users (username, email, password, verified) VALUES 
('testuser1', 'testuser1@example.com', 'password123', true),
('testuser2', 'testuser2@example.com', 'password123', true),
('testuser3', 'testuser3@example.com', 'password123', true),
('alice', 'alice@example.com', 'password123', true),
('bob', 'bob@example.com', 'password123', true)
ON CONFLICT (username) DO NOTHING;

-- 查看创建的用户
SELECT id, username, email, verified, create_time FROM users 
WHERE username IN ('testuser1', 'testuser2', 'testuser3', 'alice', 'bob')
ORDER BY id;
EOF

echo "SQL命令已准备好，请根据您的Docker配置选择合适的执行方法。"
