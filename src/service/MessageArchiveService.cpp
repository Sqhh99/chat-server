#include "MessageArchiveService.h"
#include "RedisService.h"
#include <pqxx/pqxx>
#include <json/json.h>
#include <chrono>
#include <thread>
#include <muduo/base/Logging.h>

MessageArchiveService& MessageArchiveService::getInstance() {
    static MessageArchiveService instance;
    return instance;
}

MessageArchiveService::MessageArchiveService() : running_(false) {}

MessageArchiveService::~MessageArchiveService() {
    stop();
}

bool MessageArchiveService::init() {
    // TODO: 初始化数据库连接
    return true;
}

void MessageArchiveService::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        LOG_INFO << "Message archive service is already running";
        return;
    }

    running_ = true;
    archiveThread_ = std::make_unique<std::thread>(&MessageArchiveService::archiveThread, this);
    LOG_INFO << "Message archive service started";
}

void MessageArchiveService::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        running_ = false;
        cv_.notify_one();
    }

    if (archiveThread_ && archiveThread_->joinable()) {
        archiveThread_->join();
        archiveThread_.reset();
    }
    LOG_INFO << "Message archive service stopped";
}

void MessageArchiveService::archiveThread() {
    while (running_) {
        // 执行归档操作
        bool success = archiveMessages();
        if (success) {
            LOG_INFO << "Messages archived successfully";
        } else {
            LOG_ERROR << "Failed to archive messages";
        }

        // 休眠指定时间
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::seconds(ARCHIVE_INTERVAL), [this]() { return !running_; });
    }
}

bool MessageArchiveService::archiveMessages() {
    try {
        LOG_INFO << "开始执行消息归档...";
        bool privateSuccess = true;
        bool groupSuccess = true;
        bool friendsSuccess = true;
        
        try {
            privateSuccess = archivePrivateMessages();
            if (privateSuccess) {
                LOG_INFO << "私聊消息归档成功";
            } else {
                LOG_ERROR << "私聊消息归档失败";
            }
        } catch (const std::exception& e) {
            LOG_ERROR << "私聊消息归档异常: " << e.what();
            privateSuccess = false;
        }
        
        try {
            groupSuccess = archiveGroupMessages();
            if (groupSuccess) {
                LOG_INFO << "群聊消息归档成功";
            } else {
                LOG_ERROR << "群聊消息归档失败";
            }
        } catch (const std::exception& e) {
            LOG_ERROR << "群聊消息归档异常: " << e.what();
            groupSuccess = false;
        }
        
        try {
            friendsSuccess = archiveFriendships();
            if (friendsSuccess) {
                LOG_INFO << "好友关系归档成功";
            } else {
                LOG_ERROR << "好友关系归档失败";
            }
        } catch (const std::exception& e) {
            LOG_ERROR << "好友关系归档异常: " << e.what();
            friendsSuccess = false;
        }
        
        return privateSuccess || groupSuccess || friendsSuccess; // 只要有一种归档成功，就返回true
    } catch (const std::exception& e) {
        LOG_ERROR << "Archive messages error: " << e.what();
        return false;
    }
}

bool MessageArchiveService::archivePrivateMessages() {
    try {
        // 创建数据库连接
        pqxx::connection conn("host=localhost port=5432 dbname=chat_server user=sqhh99 password=2932897504xu");
        
        // 获取所有聊天会话
        auto& redis = RedisService::getInstance();
        
        // 从Redis获取所有聊天会话键
        // 这里假设Redis中以chat:userId1:userId2格式存储私聊消息
        std::vector<std::string> chatKeys = redis.getKeys("chat:*");
        
        for (const auto& chatKey : chatKeys) {
            // 获取上次归档时间
            long long lastArchiveTime = getLastArchiveTime(chatKey);
            
            // 解析用户ID
            size_t pos1 = chatKey.find(':');
            size_t pos2 = chatKey.find(':', pos1 + 1);
            if (pos1 == std::string::npos || pos2 == std::string::npos) {
                continue;
            }
            
            // 这些用户ID可以用于归档日志或未来扩展功能
            // int userId1 = std::stoi(chatKey.substr(pos1 + 1, pos2 - pos1 - 1));
            // int userId2 = std::stoi(chatKey.substr(pos2 + 1));
            
            // 获取需要归档的消息
            std::vector<std::string> messages = redis.getAllListItems(chatKey);
            
            if (messages.empty()) {
                continue;
            }
            
            // 开始事务
            pqxx::work txn(conn);
            
            // 当前时间戳，用于记录归档时间
            long long currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            
            // 处理每条消息
            for (const auto& messageStr : messages) {
                Json::Value message;
                Json::Reader reader;
                if (!reader.parse(messageStr, message)) {
                    LOG_ERROR << "Failed to parse message JSON: " << messageStr;
                    continue;
                }
                
                // 获取消息时间戳
                long long timestamp = message["timestamp"].asInt64();
                
                // 跳过已归档的消息
                if (timestamp <= lastArchiveTime) {
                    continue;
                }
                
                // 插入消息到数据库
                txn.exec_params(
                    "INSERT INTO private_messages (from_user_id, to_user_id, content, timestamp, message_type) "
                    "VALUES ($1, $2, $3, TO_TIMESTAMP($4::bigint/1000), $5)",
                    message["from"].asString(),
                    message["to"].asString(),
                    message["content"].asString(),
                    std::to_string(timestamp),
                    message["type"].asString());
            }
            
            // 提交事务
            txn.commit();
            
            // 更新最后归档时间
            updateLastArchiveTime(chatKey, currentTime);
            
            // 清理已归档的消息
            cleanupArchivedMessages(chatKey, currentTime);
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR << "Archive private messages error: " << e.what();
        return false;
    }
}

bool MessageArchiveService::archiveGroupMessages() {
    try {
        // 创建数据库连接
        pqxx::connection conn("host=localhost port=5432 dbname=chat_server user=sqhh99 password=2932897504xu");
        
        // 获取所有群组
        std::vector<std::string> groupMsgKeys;
        auto& redis = RedisService::getInstance();
        
        // 从Redis获取所有群组消息键
        groupMsgKeys = redis.getKeys("group:*:messages");
        
        for (const auto& groupMsgKey : groupMsgKeys) {
            // 获取上次归档时间
            long long lastArchiveTime = getLastArchiveTime(groupMsgKey);
            
            // 解析群组ID
            size_t pos1 = groupMsgKey.find(':');
            size_t pos2 = groupMsgKey.find(':', pos1 + 1);
            if (pos1 == std::string::npos || pos2 == std::string::npos) {
                LOG_WARN << "Invalid group message key format: " << groupMsgKey << ". Skipping archive.";
                continue;
            }
            
            // 提取群组ID
            std::string groupIdStr = groupMsgKey.substr(pos1 + 1, pos2 - pos1 - 1);
            int groupId;
            try {
                groupId = std::stoi(groupIdStr);
            } catch (const std::exception& e) {
                LOG_ERROR << "Invalid group ID in key " << groupMsgKey << ": " << e.what() << ". Skipping archive.";
                continue;
            }
            
            // 检查键是否存在且是列表类型
            if (!redis.keyExists(groupMsgKey)) {
                LOG_WARN << "Key " << groupMsgKey << " does not exist. Skipping archive.";
                continue;
            }
            
            if (!redis.isListType(groupMsgKey)) {
                LOG_WARN << "Key " << groupMsgKey << " is not a list type. Skipping archive.";
                // 可能需要清理此键，因为它类型错误
                // redis.delKey(groupMsgKey);
                continue;
            }
            
            // 首先验证数据库中群组是否存在
            pqxx::work txn(conn);
            pqxx::result groupExists = txn.exec_params(
                "SELECT id FROM groups WHERE id = $1",
                groupId
            );
            
            // 如果群组不存在，跳过归档
            if (groupExists.empty()) {
                LOG_WARN << "Group with ID " << groupId << " does not exist in the database. Skipping archive.";
                txn.abort(); // 确保事务被回滚
                continue;
            }
            txn.commit(); // 提交查询事务
            
            // 获取需要归档的消息
            std::vector<std::string> messages = redis.getAllListItems(groupMsgKey);
            
            if (messages.empty()) {
                continue;
            }
            
            // 开始新的事务
            pqxx::work insertTxn(conn);
            
            // 当前时间戳，用于记录归档时间
            long long currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            
            // 处理每条消息
            for (const auto& messageStr : messages) {
                Json::Value message;
                Json::Reader reader;
                if (!reader.parse(messageStr, message)) {
                    LOG_ERROR << "Failed to parse message JSON: " << messageStr;
                    continue;
                }
                
                // 获取消息时间戳
                long long timestamp = message["timestamp"].asInt64();
                
                // 跳过已归档的消息
                if (timestamp <= lastArchiveTime) {
                    continue;
                }
                
                // 检查消息中的群组ID是否与键中一致
                if (!message.isMember("group") || message["group"].asString() != groupIdStr) {
                    LOG_WARN << "Message group ID mismatch or missing. Key: " << groupMsgKey 
                             << ", Message: " << messageStr << ". Skipping this message.";
                    continue;
                }
                
                // 检查发送者是否存在
                if (!message.isMember("from")) {
                    LOG_WARN << "Message missing sender ID: " << messageStr << ". Skipping this message.";
                    continue;
                }
                
                try {
                    // 插入消息到数据库
                    insertTxn.exec_params(
                        "INSERT INTO group_messages (group_id, from_user_id, content, timestamp, message_type) "
                        "VALUES ($1, $2, $3, TO_TIMESTAMP($4::bigint/1000), $5)",
                        message["group"].asString(),
                        message["from"].asString(),
                        message["content"].asString(),
                        std::to_string(timestamp),
                        message["type"].asString());
                } catch (const std::exception& e) {
                    LOG_ERROR << "Error inserting group message: " << e.what() 
                              << ". Message: " << messageStr;
                    continue; // 继续处理其他消息，而不是整体失败
                }
            }
            
            try {
                // 提交事务
                insertTxn.commit();
                
                // 更新最后归档时间
                updateLastArchiveTime(groupMsgKey, currentTime);
                
                // 清理已归档的消息
                cleanupArchivedMessages(groupMsgKey, currentTime);
                
                LOG_INFO << "Successfully archived messages for group " << groupId;
            } catch (const std::exception& e) {
                LOG_ERROR << "Archive group messages error: " << e.what() << " for group " << groupId;
                // 事务自动回滚，无需额外操作
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR << "Archive group messages error: " << e.what();
        return false;
    }
}

// 归档好友关系到数据库
bool MessageArchiveService::archiveFriendships() {
    try {
        LOG_INFO << "开始归档好友关系...";
        
        // 创建数据库连接
        pqxx::connection conn("host=localhost port=5432 dbname=chat_server user=sqhh99 password=2932897504xu");
        auto& redis = RedisService::getInstance();
        
        // 获取所有好友关系键
        std::vector<std::string> friendsKeys = redis.getKeys("user:*:friends");
        
        for (const auto& friendsKey : friendsKeys) {
            // 解析用户ID
            size_t pos1 = friendsKey.find(':');
            size_t pos2 = friendsKey.find(':', pos1 + 1);
            if (pos1 == std::string::npos || pos2 == std::string::npos) {
                LOG_WARN << "Invalid friends key format: " << friendsKey << ". Skipping.";
                continue;
            }
            
            std::string userIdStr = friendsKey.substr(pos1 + 1, pos2 - pos1 - 1);
            int userId;
            try {
                userId = std::stoi(userIdStr);
            } catch (const std::exception& e) {
                LOG_ERROR << "Invalid user ID in key " << friendsKey << ": " << e.what() << ". Skipping.";
                continue;
            }
            
            // 获取好友列表
            std::vector<int> friendIds = redis.getUserFriends(userId);
            if (friendIds.empty()) {
                LOG_INFO << "User " << userId << " has no friends. Skipping.";
                continue;
            }
            
            LOG_INFO << "Found " << friendIds.size() << " friends for user " << userId;
            
            // 创建事务
            pqxx::work txn(conn);
            
            // 处理每个好友关系
            for (int friendId : friendIds) {
                // 检查该好友关系是否已存在于数据库
                pqxx::result exists = txn.exec_params(
                    "SELECT id FROM user_friends WHERE "
                    "(user_id1 = $1 AND user_id2 = $2) OR (user_id1 = $2 AND user_id2 = $1)",
                    userId, friendId
                );
                
                // 如果关系不存在，则插入
                if (exists.empty()) {
                    LOG_INFO << "Adding friendship between " << userId << " and " << friendId << " to database";
                    
                    // 确保用户ID顺序一致（较小ID放在前面）
                    int smallerId = (userId < friendId) ? userId : friendId;
                    int largerId = (userId > friendId) ? userId : friendId;
                    
                    try {
                        txn.exec_params(
                            "INSERT INTO user_friends (user_id1, user_id2, status, created_at, updated_at) "
                            "VALUES ($1, $2, 'accepted', NOW(), NOW())",
                            smallerId, largerId
                        );
                    } catch (const std::exception& e) {
                        LOG_ERROR << "Failed to insert friendship: " << e.what();
                        // 继续处理其他好友
                    }
                }
            }
            
            // 提交事务
            try {
                txn.commit();
                LOG_INFO << "Successfully archived friendships for user " << userId;
            } catch (const std::exception& e) {
                LOG_ERROR << "Failed to commit friendship transaction: " << e.what();
            }
        }
        
        LOG_INFO << "好友关系归档完成";
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR << "归档好友关系异常: " << e.what();
        return false;
    }
}

long long MessageArchiveService::getLastArchiveTime(const std::string& key) {
    try {
        std::string archiveKey = key + ":last_archive";
        std::string timeStr;
        
        auto& redis = RedisService::getInstance();
        if (!redis.keyExists(archiveKey)) {
            return 0;
        }
        
        timeStr = redis.getValue(archiveKey, "0");
        return std::stoll(timeStr);
    } catch (const std::exception& e) {
        LOG_ERROR << "Get last archive time error: " << e.what();
        return 0;
    }
}

bool MessageArchiveService::updateLastArchiveTime(const std::string& key, long long timestamp) {
    try {
        std::string archiveKey = key + ":last_archive";
        RedisService::getInstance().setValue(archiveKey, std::to_string(timestamp));
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR << "Update last archive time error: " << e.what();
        return false;
    }
}

bool MessageArchiveService::cleanupArchivedMessages(const std::string& key, long long /* timestamp */) {
    try {
        // 此处可以实现删除已归档的消息，或者保留最近的N条
        // 为了安全起见，我们这里只保留最近的100条消息
        // timestamp参数预留给未来按时间戳清理的功能使用
        RedisService::getInstance().trimList(key, -100, -1);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR << "Cleanup archived messages error: " << e.what();
        return false;
    }
}

std::vector<std::string> MessageArchiveService::getHistoricalMessages(int userId1, int userId2, int count, int offset) {
    std::vector<std::string> messages;
    
    try {
        // 确保用户ID顺序一致
        if (userId1 > userId2) {
            std::swap(userId1, userId2);
        }
        
        // 创建数据库连接
        pqxx::connection conn("host=localhost port=5432 dbname=chat_server user=sqhh99 password=2932897504xu");
        pqxx::work txn(conn);
        
        // 查询历史消息
        pqxx::result result = txn.exec_params(
            "SELECT from_user_id, to_user_id, content, EXTRACT(EPOCH FROM timestamp) * 1000 as ts, message_type "
            "FROM private_messages "
            "WHERE (from_user_id = $1 AND to_user_id = $2) OR (from_user_id = $2 AND to_user_id = $1) "
            "ORDER BY timestamp DESC "
            "LIMIT $3 OFFSET $4",
            std::to_string(userId1),
            std::to_string(userId2),
            std::to_string(count),
            std::to_string(offset)
        );
        
        // 将结果转换为JSON格式
        for (const auto& row : result) {
            Json::Value message;
            message["from"] = row["from_user_id"].as<int>();
            message["to"] = row["to_user_id"].as<int>();
            message["content"] = row["content"].as<std::string>();
            message["timestamp"] = static_cast<Json::UInt64>(row["ts"].as<double>());
            message["type"] = "private";
            
            Json::StreamWriterBuilder writer;
            messages.push_back(Json::writeString(writer, message));
        }
        
        txn.commit();
    } catch (const std::exception& e) {
        LOG_ERROR << "Get historical messages error: " << e.what();
    }
    
    return messages;
}

std::vector<std::string> MessageArchiveService::getHistoricalGroupMessages(int groupId, int count, int offset) {
    std::vector<std::string> messages;
    
    try {
        // 创建数据库连接
        pqxx::connection conn("host=localhost port=5432 dbname=chat_server user=sqhh99 password=2932897504xu");
        pqxx::work txn(conn);
        
        // 查询历史消息
        pqxx::result result = txn.exec_params(
            "SELECT group_id, from_user_id, content, EXTRACT(EPOCH FROM timestamp) * 1000 as ts, message_type "
            "FROM group_messages "
            "WHERE group_id = $1 "
            "ORDER BY timestamp DESC "
            "LIMIT $2 OFFSET $3",
            std::to_string(groupId),
            std::to_string(count),
            std::to_string(offset)
        );
        
        // 将结果转换为JSON格式
        for (const auto& row : result) {
            Json::Value message;
            message["from"] = row["from_user_id"].as<int>();
            message["group"] = row["group_id"].as<int>();
            message["content"] = row["content"].as<std::string>();
            message["timestamp"] = static_cast<Json::UInt64>(row["ts"].as<double>());
            message["type"] = "group";
            
            Json::StreamWriterBuilder writer;
            messages.push_back(Json::writeString(writer, message));
        }
        
        txn.commit();
    } catch (const std::exception& e) {
        LOG_ERROR << "Get historical group messages error: " << e.what();
    }
    
    return messages;
}
