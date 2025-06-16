#!/bin/bash

# 数据库配置
DB_NAME="chat_server"
DB_USER="sqhh99"
DB_PASSWORD="2932897504xu"

# 确认函数
confirm() {
    read -r -p "$1 [y/N] " response
    case "$response" in
        [yY][eE][sS]|[yY]) 
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

# 连接到数据库
echo "正在连接到数据库 $DB_NAME..."
PGPASSWORD="$DB_PASSWORD" psql -U "$DB_USER" -d "$DB_NAME" -c "SELECT version()" > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "错误: 无法连接到数据库，请检查连接参数"
    exit 1
fi

echo "===== 数据库清理工具 ====="
echo "此脚本将帮助清理不再使用的旧表"
echo "在执行之前，请确保所有相关代码都已更新为使用新表结构"

echo -e "\n正在检查旧表是否存在..."

# 检查旧表存在情况
PGPASSWORD="$DB_PASSWORD" psql -U "$DB_USER" -d "$DB_NAME" -t -c "
SELECT EXISTS (SELECT FROM information_schema.tables WHERE table_name = 'chats');
SELECT EXISTS (SELECT FROM information_schema.tables WHERE table_name = 'chat_members');
SELECT EXISTS (SELECT FROM information_schema.tables WHERE table_name = 'messages');" > /tmp/tables_exist.txt

CHATS_EXISTS=$(sed -n '1p' /tmp/tables_exist.txt | tr -d ' ')
CHAT_MEMBERS_EXISTS=$(sed -n '2p' /tmp/tables_exist.txt | tr -d ' ')
MESSAGES_EXISTS=$(sed -n '3p' /tmp/tables_exist.txt | tr -d ' ')

if [ "$CHATS_EXISTS" == "t" ]; then
    echo " - 表 'chats' 存在"
else
    echo " - 表 'chats' 不存在"
fi

if [ "$CHAT_MEMBERS_EXISTS" == "t" ]; then
    echo " - 表 'chat_members' 存在"
else
    echo " - 表 'chat_members' 不存在"
fi

if [ "$MESSAGES_EXISTS" == "t" ]; then
    echo " - 表 'messages' 存在"
else
    echo " - 表 'messages' 不存在"
fi

echo -e "\n检查现有数据..."

# 检查旧表中是否有数据
if [ "$MESSAGES_EXISTS" == "t" ]; then
    MESSAGE_COUNT=$(PGPASSWORD="$DB_PASSWORD" psql -U "$DB_USER" -d "$DB_NAME" -t -c "SELECT COUNT(*) FROM messages;" | tr -d ' ')
    echo " - 表 'messages' 中有 $MESSAGE_COUNT 条记录"
fi

if [ "$CHATS_EXISTS" == "t" ]; then
    CHATS_COUNT=$(PGPASSWORD="$DB_PASSWORD" psql -U "$DB_USER" -d "$DB_NAME" -t -c "SELECT COUNT(*) FROM chats;" | tr -d ' ')
    echo " - 表 'chats' 中有 $CHATS_COUNT 条记录"
fi

if [ "$CHAT_MEMBERS_EXISTS" == "t" ]; then
    CHAT_MEMBERS_COUNT=$(PGPASSWORD="$DB_PASSWORD" psql -U "$DB_USER" -d "$DB_NAME" -t -c "SELECT COUNT(*) FROM chat_members;" | tr -d ' ')
    echo " - 表 'chat_members' 中有 $CHAT_MEMBERS_COUNT 条记录"
fi

echo -e "\n警告: 删除这些表将永久丢失其中的所有数据！"
echo "在继续之前，请确保所有必要的数据已经被迁移到新表中。"

if confirm "是否确认删除旧表结构？"; then
    echo "开始删除旧表..."
    
    # 删除表
    PGPASSWORD="$DB_PASSWORD" psql -U "$DB_USER" -d "$DB_NAME" -c "
    DROP TABLE IF EXISTS messages;
    DROP TABLE IF EXISTS chat_members;
    DROP TABLE IF EXISTS chats;
    "
    
    echo "旧表删除成功！"
else
    echo "操作已取消。没有任何表被删除。"
fi

echo -e "\n为确保新表结构完整，正在更新数据库结构..."
echo "应用最新的数据库架构..."

if confirm "是否更新到最新的表结构？"; then
    PGPASSWORD="$DB_PASSWORD" psql -U "$DB_USER" -d "$DB_NAME" -f ./database/messages_schema_updated.sql
    echo "数据库结构已更新！"
else
    echo "数据库结构更新已取消。"
fi

echo -e "\n清理脚本执行完毕！"
