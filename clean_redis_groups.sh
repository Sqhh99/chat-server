#!/bin/bash

# 此脚本用于清理Redis中引用了不存在数据库群组的消息键
# 使用方式:
#   ./clean_redis_groups.sh

# 配置
REDIS_HOST="127.0.0.1"
REDIS_PORT="6379"
PSQL_CONN="host=localhost port=5432 dbname=chat_server user=sqhh99 password=2932897504xu"

echo "开始清理Redis中无效的群组消息键..."

# 通过redis-cli获取所有群组消息键
group_keys=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT keys "group:*:messages")

for key in $group_keys; do
    # 从键中提取群组ID
    group_id=$(echo $key | sed -n 's/group:\([0-9]\+\):messages/\1/p')
    
    if [ -n "$group_id" ]; then
        # 检查数据库中是否存在该群组
        exists=$(psql "$PSQL_CONN" -t -c "SELECT 1 FROM groups WHERE id = $group_id LIMIT 1;")
        
        if [ -z "$exists" ]; then
            echo "群组ID $group_id 在数据库中不存在，删除键 $key"
            redis-cli -h $REDIS_HOST -p $REDIS_PORT del "$key"
            
            # 同时删除最后归档时间
            archive_key="archive:lasttime:$key"
            redis-cli -h $REDIS_HOST -p $REDIS_PORT del "$archive_key"
            
            echo "已删除: $key 和 $archive_key"
        else
            echo "群组ID $group_id 在数据库中存在，保留键 $key"
            
            # 检查键的类型
            key_type=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT type "$key")
            if [ "$key_type" != "list" ]; then
                echo "警告: 键 $key 类型错误 (应为list，实为 $key_type)，删除此键"
                redis-cli -h $REDIS_HOST -p $REDIS_PORT del "$key"
            fi
        fi
    fi
done

echo "Redis群组消息键清理完成"
