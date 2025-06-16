#include "RedisService.h"
#include <chrono>
#include <ctime>
#include <json/json.h>

RedisService &RedisService::getInstance()
{
    static RedisService instance;
    return instance;
}

RedisService::RedisService() : initialized_(false) {}

RedisService::~RedisService() {}

bool RedisService::init(const std::string &host, int port, const std::string &password, int db)
{
    try
    {
        // 构建连接URI
        std::string uri = "tcp://" + host + ":" + std::to_string(port);

        // 创建连接选项
        sw::redis::ConnectionOptions conn_options;
        conn_options.host = host;
        conn_options.port = port;
        conn_options.db = db;

        if (!password.empty())
        {
            conn_options.password = password;
        }

        // 设置连接池选项
        sw::redis::ConnectionPoolOptions pool_options;
        pool_options.size = 5;                                      // 连接池大小
        pool_options.wait_timeout = std::chrono::milliseconds(100); // 连接池等待超时

        // 创建Redis客户端
        redis_ = std::make_unique<sw::redis::Redis>(conn_options, pool_options);

        // 测试连接
        try
        {
            redis_->ping();
            LOG_INFO << "Redis connection established successfully at " << host << ":" << port;
            initialized_ = true;
            return true;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Redis ping failed: " << e.what();
            return false;
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Redis connection failed: " << e.what();
        return false;
    }
}

std::string RedisService::getChatKey(int userId1, int userId2)
{
    // 保证较小的用户ID在前，确保两个用户能获取到相同的key
    if (userId1 > userId2)
    {
        std::swap(userId1, userId2);
    }
    return "chat:" + std::to_string(userId1) + ":" + std::to_string(userId2);
}

std::string RedisService::getGroupKey(int groupId)
{
    return "group:" + std::to_string(groupId);
}

std::string RedisService::getGroupMembersKey(int groupId)
{
    return "group:" + std::to_string(groupId) + ":members";
}

std::string RedisService::getUserGroupsKey(int userId)
{
    return "user:" + std::to_string(userId) + ":groups";
}

std::string RedisService::getUserFriendsKey(int userId)
{
    return "user:" + std::to_string(userId) + ":friends";
}

bool RedisService::sendPrivateMessage(int fromUserId, int toUserId, const std::string &content)
{
    if (!initialized_ || !redis_)
        return false;

    try
    {
        // 构建消息JSON
        Json::Value message;
        message["from"] = fromUserId;
        message["to"] = toUserId;
        message["content"] = content;
        message["timestamp"] = static_cast<Json::UInt64>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
        message["type"] = "private";

        // 转换为字符串
        Json::StreamWriterBuilder writer;
        std::string messageStr = Json::writeString(writer, message);

        // 获取聊天key
        std::string chatKey = getChatKey(fromUserId, toUserId);

        // 将消息添加到聊天列表
        redis_->rpush(chatKey, messageStr);

        // 限制列表大小，保留最近的100条消息
        redis_->ltrim(chatKey, -100, -1);

        // 如果用户不在线，将消息加入离线消息队列
        if (!isUserOnline(toUserId))
        {
            std::string offlineKey = "user:" + std::to_string(toUserId) + ":offline";
            redis_->rpush(offlineKey, messageStr);
        }

        LOG_INFO << "Private message sent from user " << fromUserId << " to user " << toUserId;
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to send private message: " << e.what();
        return false;
    }
}

bool RedisService::sendGroupMessage(int fromUserId, int groupId, const std::string &content)
{
    if (!initialized_ || !redis_)
        return false;

    try
    {
        // 检查群组是否存在
        std::string groupKey = getGroupKey(groupId);
        if (!redis_->exists(groupKey))
        {
            LOG_ERROR << "Group " << groupId << " does not exist";
            return false;
        }

        // 检查用户是否在群组中
        std::string groupMembersKey = getGroupMembersKey(groupId);
        if (!redis_->sismember(groupMembersKey, std::to_string(fromUserId)))
        {
            LOG_ERROR << "User " << fromUserId << " is not a member of group " << groupId;
            return false;
        }

        // 构建消息JSON
        Json::Value message;
        message["from"] = fromUserId;
        message["group"] = groupId;
        message["content"] = content;
        message["timestamp"] = static_cast<Json::UInt64>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
        message["type"] = "group";

        // 转换为字符串
        Json::StreamWriterBuilder writer;
        std::string messageStr = Json::writeString(writer, message);

        // 将消息添加到群组消息列表
        std::string groupMsgKey = "group:" + std::to_string(groupId) + ":messages";
        redis_->rpush(groupMsgKey, messageStr);

        // 限制列表大小，保留最近的200条消息
        redis_->ltrim(groupMsgKey, -200, -1);

        // 获取群组所有成员
        std::vector<std::string> members;
        redis_->smembers(groupMembersKey, std::back_inserter(members));

        // 遍历群组成员，如果不在线，将消息加入离线消息队列
        for (const auto &memberStr : members)
        {
            int memberId = std::stoi(memberStr);
            if (memberId != fromUserId && !isUserOnline(memberId))
            {
                std::string offlineKey = "user:" + memberStr + ":offline";
                redis_->rpush(offlineKey, messageStr);
            }
        }

        LOG_INFO << "Group message sent from user " << fromUserId << " to group " << groupId;
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to send group message: " << e.what();
        return false;
    }
}

std::vector<std::string> RedisService::getPrivateMessages(int userId1, int userId2, int count)
{
    std::vector<std::string> messages;
    if (!initialized_ || !redis_)
        return messages;

    try
    {
        // 获取聊天key
        std::string chatKey = getChatKey(userId1, userId2);

        // 获取最近的count条消息
        redis_->lrange(chatKey, -count, -1, std::back_inserter(messages));

        return messages;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to get private messages: " << e.what();
        return std::vector<std::string>();
    }
}

std::vector<std::string> RedisService::getGroupMessages(int groupId, int count)
{
    std::vector<std::string> messages;
    if (!initialized_ || !redis_)
        return messages;

    try
    {
        // 获取群组消息key
        std::string groupMsgKey = "group:" + std::to_string(groupId) + ":messages";

        // 获取最近的count条消息
        redis_->lrange(groupMsgKey, -count, -1, std::back_inserter(messages));

        return messages;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to get group messages: " << e.what();
        return std::vector<std::string>();
    }
}

std::vector<int> RedisService::getUserChats(int userId)
{
    std::vector<int> chats;
    if (!initialized_ || !redis_)
        return chats;

    try
    {
        // 获取用户所有的聊天key
        std::string pattern = "chat:" + std::to_string(userId) + ":*";
        std::vector<std::string> keys;
        redis_->keys(pattern, std::back_inserter(keys));

        // 解析每个key以获取另一个用户的ID
        for (const auto &key : keys)
        {
            // key格式: chat:userId1:userId2
            size_t pos1 = key.find(':');
            size_t pos2 = key.find(':', pos1 + 1);
            if (pos1 != std::string::npos && pos2 != std::string::npos)
            {
                int userId1 = std::stoi(key.substr(pos1 + 1, pos2 - pos1 - 1));
                int userId2 = std::stoi(key.substr(pos2 + 1));

                // 添加不是当前用户的那个ID
                if (userId1 == userId)
                {
                    chats.push_back(userId2);
                }
                else
                {
                    chats.push_back(userId1);
                }
            }
        }

        return chats;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to get user chats: " << e.what();
        return std::vector<int>();
    }
}

std::vector<int> RedisService::getUserGroups(int userId)
{
    std::vector<int> groups;
    if (!initialized_ || !redis_)
        return groups;

    try
    {
        // 获取用户的群组key
        std::string userGroupsKey = getUserGroupsKey(userId);

        // 获取用户加入的所有群组
        std::vector<std::string> groupStrs;
        redis_->smembers(userGroupsKey, std::back_inserter(groupStrs));

        // 转换为int
        for (const auto &groupStr : groupStrs)
        {
            groups.push_back(std::stoi(groupStr));
        }

        return groups;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to get user groups: " << e.what();
        return std::vector<int>();
    }
}

bool RedisService::createGroup(int groupId, const std::string &groupName, int creatorId)
{
    if (!initialized_ || !redis_)
        return false;

    try
    {
        // 获取群组key
        std::string groupKey = getGroupKey(groupId);

        // 检查群组是否已存在
        if (redis_->exists(groupKey))
        {
            LOG_ERROR << "Group " << groupId << " already exists";
            return false;
        }

        // 创建群组信息
        std::string timestamp = std::to_string(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());

        // 设置群组信息
        redis_->hset(groupKey, "name", groupName);
        redis_->hset(groupKey, "creator", std::to_string(creatorId));
        redis_->hset(groupKey, "createTime", timestamp);

        // 添加创建者到群组成员
        std::string groupMembersKey = getGroupMembersKey(groupId);
        redis_->sadd(groupMembersKey, std::to_string(creatorId));

        // 将群组添加到创建者的群组列表
        std::string userGroupsKey = getUserGroupsKey(creatorId);
        redis_->sadd(userGroupsKey, std::to_string(groupId));

        LOG_INFO << "Group " << groupId << " created by user " << creatorId;
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to create group: " << e.what();
        return false;
    }
}

bool RedisService::joinGroup(int userId, int groupId)
{
    if (!initialized_ || !redis_)
        return false;

    try
    {
        // 检查群组是否存在
        std::string groupKey = getGroupKey(groupId);
        if (!redis_->exists(groupKey))
        {
            LOG_ERROR << "Group " << groupId << " does not exist";
            return false;
        }

        // 添加用户到群组成员
        std::string groupMembersKey = getGroupMembersKey(groupId);
        redis_->sadd(groupMembersKey, std::to_string(userId));

        // 将群组添加到用户的群组列表
        std::string userGroupsKey = getUserGroupsKey(userId);
        redis_->sadd(userGroupsKey, std::to_string(groupId));

        LOG_INFO << "User " << userId << " joined group " << groupId;
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to join group: " << e.what();
        return false;
    }
}

bool RedisService::leaveGroup(int userId, int groupId)
{
    if (!initialized_ || !redis_)
        return false;

    try
    {
        // 检查群组是否存在
        std::string groupKey = getGroupKey(groupId);
        if (!redis_->exists(groupKey))
        {
            LOG_ERROR << "Group " << groupId << " does not exist";
            return false;
        }

        // 检查用户是否在群组中
        std::string groupMembersKey = getGroupMembersKey(groupId);
        if (!redis_->sismember(groupMembersKey, std::to_string(userId)))
        {
            LOG_ERROR << "User " << userId << " is not a member of group " << groupId;
            return false;
        }

        // 移除用户从群组成员
        redis_->srem(groupMembersKey, std::to_string(userId));

        // 将群组从用户的群组列表移除
        std::string userGroupsKey = getUserGroupsKey(userId);
        redis_->srem(userGroupsKey, std::to_string(groupId));

        // 检查是否是创建者，如果是且群组没有其他成员，则删除群组
        auto creator_opt = redis_->hget(groupKey, "creator");
        std::string creator;
        if (creator_opt)
        {
            creator = *creator_opt;
        }

        if (creator == std::to_string(userId))
        {
            long long memberCount = redis_->scard(groupMembersKey);
            if (memberCount == 0)
            {
                // 删除群组
                redis_->del(groupKey);
                redis_->del(groupMembersKey);

                // 删除群组消息
                std::string groupMsgKey = "group:" + std::to_string(groupId) + ":messages";
                redis_->del(groupMsgKey);

                LOG_INFO << "Group " << groupId << " deleted as creator left and no members remain";
            }
        }

        LOG_INFO << "User " << userId << " left group " << groupId;
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to leave group: " << e.what();
        return false;
    }
}

std::vector<int> RedisService::getGroupMembers(int groupId)
{
    std::vector<int> members;
    if (!initialized_ || !redis_)
        return members;

    try
    {
        // 获取群组成员key
        std::string groupMembersKey = getGroupMembersKey(groupId);

        // 检查群组是否存在
        if (!redis_->exists(getGroupKey(groupId)))
        {
            LOG_ERROR << "Group " << groupId << " does not exist";
            return members;
        }

        // 获取所有成员
        std::vector<std::string> memberStrs;
        redis_->smembers(groupMembersKey, std::back_inserter(memberStrs));

        // 转换为int
        for (const auto &memberStr : memberStrs)
        {
            members.push_back(std::stoi(memberStr));
        }

        return members;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to get group members: " << e.what();
        return std::vector<int>();
    }
}

bool RedisService::setUserOnline(int userId, bool online)
{
    if (!initialized_ || !redis_)
        return false;

    try
    {
        if (online)
        {
            // 添加到在线用户集合
            redis_->sadd(ONLINE_USERS_KEY, std::to_string(userId));

            // 设置用户在线状态，过期时间2分钟
            std::string userOnlineKey = "user:" + std::to_string(userId) + ":online";
            redis_->setex(userOnlineKey, 120, "1");

            LOG_INFO << "User " << userId << " is now online";
        }
        else
        {
            // 从在线用户集合中移除
            redis_->srem(ONLINE_USERS_KEY, std::to_string(userId));

            // 删除用户在线状态
            std::string userOnlineKey = "user:" + std::to_string(userId) + ":online";
            redis_->del(userOnlineKey);

            LOG_INFO << "User " << userId << " is now offline";
        }
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to set user online status: " << e.what();
        return false;
    }
}

bool RedisService::isUserOnline(int userId)
{
    if (!initialized_ || !redis_)
        return false;

    try
    {
        // 检查用户是否在在线用户集合中
        return redis_->sismember(ONLINE_USERS_KEY, std::to_string(userId));
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to check if user is online: " << e.what();
        return false;
    }
}

std::vector<int> RedisService::getOnlineUsers()
{
    std::vector<int> onlineUsers;
    if (!initialized_ || !redis_)
        return onlineUsers;

    try
    {
        // 获取所有在线用户
        std::vector<std::string> userStrs;
        redis_->smembers(ONLINE_USERS_KEY, std::back_inserter(userStrs));

        // 转换为int
        for (const auto &userStr : userStrs)
        {
            onlineUsers.push_back(std::stoi(userStr));
        }

        return onlineUsers;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to get online users: " << e.what();
        return std::vector<int>();
    }
}

bool RedisService::addFriend(int userId1, int userId2)
{
    if (!initialized_ || !redis_)
        return false;

    try
    {
        // 将userId2添加到userId1的好友列表
        std::string userFriendsKey1 = getUserFriendsKey(userId1);
        redis_->sadd(userFriendsKey1, std::to_string(userId2));

        // 将userId1添加到userId2的好友列表
        std::string userFriendsKey2 = getUserFriendsKey(userId2);
        redis_->sadd(userFriendsKey2, std::to_string(userId1));

        LOG_INFO << "Added friend relationship between user " << userId1 << " and user " << userId2;
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to add friend: " << e.what();
        return false;
    }
}

bool RedisService::removeFriend(int userId1, int userId2)
{
    if (!initialized_ || !redis_)
        return false;

    try
    {
        // 从userId1的好友列表移除userId2
        std::string userFriendsKey1 = getUserFriendsKey(userId1);
        redis_->srem(userFriendsKey1, std::to_string(userId2));

        // 从userId2的好友列表移除userId1
        std::string userFriendsKey2 = getUserFriendsKey(userId2);
        redis_->srem(userFriendsKey2, std::to_string(userId1));

        LOG_INFO << "Removed friend relationship between user " << userId1 << " and user " << userId2;
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to remove friend: " << e.what();
        return false;
    }
}

std::vector<int> RedisService::getUserFriends(int userId)
{
    std::vector<int> friends;
    if (!initialized_ || !redis_)
        return friends;

    try
    {
        // 获取用户的好友列表
        std::string userFriendsKey = getUserFriendsKey(userId);

        // 获取所有好友
        std::vector<std::string> friendStrs;
        redis_->smembers(userFriendsKey, std::back_inserter(friendStrs));

        // 转换为int
        for (const auto &friendStr : friendStrs)
        {
            friends.push_back(std::stoi(friendStr));
        }

        return friends;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to get user friends: " << e.what();
        return std::vector<int>();
    }
}

bool RedisService::isFriend(int userId1, int userId2)
{
    if (!initialized_ || !redis_)
        return false;

    try
    {
        // 获取用户1的好友列表key
        std::string userFriendsKey = getUserFriendsKey(userId1);

        // 检查用户2是否在用户1的好友列表中
        return redis_->sismember(userFriendsKey, std::to_string(userId2));
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to check friend relationship: " << e.what();
        return false;
    }
}

// 标记消息为已读
bool RedisService::markMessageAsRead(int userId, const std::string &messageId)
{
    if (!initialized_ || !redis_)
        return false;

    try
    {
        // 检查消息是否存在
        std::string messageKey = "message:" + messageId;
        if (!redis_->exists(messageKey))
        {
            LOG_ERROR << "Message " << messageId << " does not exist";
            return false;
        }

        // 获取消息数据
        auto messageData_opt = redis_->hget(messageKey, "data");
        if (!messageData_opt)
        {
            LOG_ERROR << "Failed to get message data for message " << messageId;
            return false;
        }
        std::string messageData = *messageData_opt;

        // 解析消息数据
        Json::Value message;
        Json::Reader reader;
        if (!reader.parse(messageData, message))
        {
            LOG_ERROR << "Failed to parse message data: " << messageData;
            return false;
        }

        // 检查用户是否是接收者
        if (message.isMember("to") && message["to"].asInt() != userId)
        {
            LOG_ERROR << "User " << userId << " is not the recipient of message " << messageId;
            return false;
        }

        // 标记消息为已读
        redis_->hset(messageKey, "read", "1");

        // 记录已读时间戳
        long long timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
        redis_->hset(messageKey, "read_timestamp", std::to_string(timestamp));

        LOG_INFO << "Message " << messageId << " marked as read by user " << userId;
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to mark message as read: " << e.what();
        return false;
    }
}

// 标记群组消息为已读
bool RedisService::markGroupMessageAsRead(int userId, int groupId, const std::string &messageId)
{
    if (!initialized_ || !redis_)
        return false;

    try
    {
        // 检查消息是否存在
        std::string messageKey = "message:" + messageId;
        if (!redis_->exists(messageKey))
        {
            LOG_ERROR << "Message " << messageId << " does not exist";
            return false;
        }

        // 获取消息数据
        auto messageData_opt = redis_->hget(messageKey, "data");
        if (!messageData_opt)
        {
            LOG_ERROR << "Failed to get message data for message " << messageId;
            return false;
        }
        std::string messageData = *messageData_opt;

        // 解析消息数据
        Json::Value message;
        Json::Reader reader;
        if (!reader.parse(messageData, message))
        {
            LOG_ERROR << "Failed to parse message data: " << messageData;
            return false;
        }

        // 检查消息是否属于指定的群组
        if (!message.isMember("group") || message["group"].asInt() != groupId)
        {
            LOG_ERROR << "Message " << messageId << " does not belong to group " << groupId;
            return false;
        }

        // 检查用户是否是群组成员
        if (!redis_->sismember(getGroupMembersKey(groupId), std::to_string(userId)))
        {
            LOG_ERROR << "User " << userId << " is not a member of group " << groupId;
            return false;
        }

        // 标记消息为已读
        std::string readKey = messageKey + ":read";
        redis_->sadd(readKey, std::to_string(userId));

        // 记录已读时间戳
        std::string readTsKey = messageKey + ":read_timestamps";
        long long timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
        redis_->hset(readTsKey, std::to_string(userId), std::to_string(timestamp));

        LOG_INFO << "Group message " << messageId << " marked as read by user " << userId;
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to mark group message as read: " << e.what();
        return false;
    }
}

// 撤回私聊消息
bool RedisService::recallPrivateMessage(int userId, int targetUserId, const std::string &messageId)
{
    if (!initialized_ || !redis_)
        return false;

    try
    {
        // 检查消息是否存在
        std::string messageKey = "message:" + messageId;
        if (!redis_->exists(messageKey))
        {
            LOG_ERROR << "Message " << messageId << " does not exist";
            return false;
        }

        // 获取消息数据
        auto messageData_opt = redis_->hget(messageKey, "data");
        if (!messageData_opt)
        {
            LOG_ERROR << "Failed to get message data for message " << messageId;
            return false;
        }
        std::string messageData = *messageData_opt;

        // 解析消息数据
        Json::Value message;
        Json::Reader reader;
        if (!reader.parse(messageData, message))
        {
            LOG_ERROR << "Failed to parse message data: " << messageData;
            return false;
        }

        // 检查用户是否是发送者
        if (!message.isMember("from") || message["from"].asInt() != userId)
        {
            LOG_ERROR << "User " << userId << " is not the sender of message " << messageId;
            return false;
        }

        // 检查消息是否在2分钟内
        long long timestamp = message["timestamp"].asInt64();
        long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();

        if (now - timestamp > 120000)
        { // 2分钟 = 120000毫秒
            LOG_ERROR << "Cannot recall message " << messageId << " after 2 minutes";
            return false;
        }

        // 标记消息为已撤回
        message["recalled"] = true;
        message["recall_time"] = static_cast<Json::UInt64>(now);

        // 更新消息
        Json::StreamWriterBuilder writer;
        std::string updatedMessageStr = Json::writeString(writer, message);
        redis_->hset(messageKey, "data", updatedMessageStr);

        // 获取聊天key
        std::string chatKey = getChatKey(userId, targetUserId);

        // 更新聊天记录
        std::vector<std::string> messages;
        redis_->lrange(chatKey, 0, -1, std::back_inserter(messages));

        for (size_t i = 0; i < messages.size(); i++)
        {
            Json::Value msg;
            if (reader.parse(messages[i], msg) && msg.isMember("id") && msg["id"].asString() == messageId)
            {
                // 将撤回的消息添加标记
                msg["recalled"] = true;
                msg["recall_time"] = static_cast<Json::UInt64>(now);

                // 更新聊天记录
                std::string updatedMsg = Json::writeString(writer, msg);
                redis_->lset(chatKey, static_cast<long long>(i), updatedMsg);
                break;
            }
        }

        LOG_INFO << "Message " << messageId << " recalled by user " << userId;
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to recall message: " << e.what();
        return false;
    }
}

// 撤回群组消息
bool RedisService::recallGroupMessage(int userId, int groupId, const std::string &messageId)
{
    if (!initialized_ || !redis_)
        return false;

    try
    {
        // 检查消息是否存在
        std::string messageKey = "message:" + messageId;
        if (!redis_->exists(messageKey))
        {
            LOG_ERROR << "Message " << messageId << " does not exist";
            return false;
        }

        // 获取消息数据
        auto messageDataOpt = redis_->hget(messageKey, "data");
        if (!messageDataOpt)
        {
            LOG_ERROR << "Failed to get message data for message " << messageId;
            return false;
        }
        std::string messageData = *messageDataOpt;

        // 解析消息数据
        Json::Value message;
        Json::Reader reader;
        if (!reader.parse(messageData, message))
        {
            LOG_ERROR << "Failed to parse message data: " << messageData;
            return false;
        }

        // 检查消息是否属于指定的群组
        if (!message.isMember("group") || message["group"].asInt() != groupId)
        {
            LOG_ERROR << "Message " << messageId << " does not belong to group " << groupId;
            return false;
        }

        // 检查用户是否是发送者或群主/管理员
        bool isSender = message.isMember("from") && message["from"].asInt() == userId;
        bool isAdmin = false;

        if (!isSender)
        {
            // 检查用户是否是群主或管理员
            std::string groupKey = getGroupKey(groupId);
            auto creatorOpt = redis_->hget(groupKey, "creator");
            std::string creator = creatorOpt ? *creatorOpt : "";

            if (creator == std::to_string(userId))
            {
                isAdmin = true;
            }
            else
            {
                // TODO: 实现群管理员角色检查
                isAdmin = false;
            }
        }

        if (!isSender && !isAdmin)
        {
            LOG_ERROR << "User " << userId << " is not allowed to recall message " << messageId;
            return false;
        }

        // 如果是发送者，检查消息是否在2分钟内
        if (isSender && !isAdmin)
        {
            long long timestamp = message["timestamp"].asInt64();
            long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();

            if (now - timestamp > 120000)
            { // 2分钟 = 120000毫秒
                LOG_ERROR << "Cannot recall message " << messageId << " after 2 minutes";
                return false;
            }
        }

        // 标记消息为已撤回
        long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();

        message["recalled"] = true;
        message["recall_time"] = static_cast<Json::UInt64>(now);
        message["recall_by"] = userId;

        // 更新消息
        Json::StreamWriterBuilder writer;
        std::string updatedMessageStr = Json::writeString(writer, message);
        redis_->hset(messageKey, "data", updatedMessageStr);

        // 获取群聊key
        std::string groupMsgKey = "group:" + std::to_string(groupId) + ":messages";

        // 更新群聊记录
        std::vector<std::string> messages;
        redis_->lrange(groupMsgKey, 0, -1, std::back_inserter(messages));

        for (size_t i = 0; i < messages.size(); i++)
        {
            Json::Value msg;
            if (reader.parse(messages[i], msg) && msg.isMember("id") && msg["id"].asString() == messageId)
            {
                // 将撤回的消息添加标记
                msg["recalled"] = true;
                msg["recall_time"] = static_cast<Json::UInt64>(now);
                msg["recall_by"] = userId;

                // 更新群聊记录
                std::string updatedMsg = Json::writeString(writer, msg);
                redis_->lset(groupMsgKey, static_cast<long long>(i), updatedMsg);
                break;
            }
        }

        LOG_INFO << "Group message " << messageId << " recalled by user " << userId;
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to recall group message: " << e.what();
        return false;
    }
}

// 删除键
bool RedisService::delKey(const std::string &key)
{
    try
    {
        if (redis_ && keyExists(key))
        {
            return redis_->del(key) > 0;
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Redis delKey error: " << e.what();
    }
    return false;
}

// 获取所有匹配的键
std::vector<std::string> RedisService::getKeys(const std::string &pattern)
{
    std::vector<std::string> keys;
    try
    {
        if (redis_)
        {
            redis_->keys(pattern, std::back_inserter(keys));
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Redis getKeys error: " << e.what();
    }
    return keys;
}

// 获取列表中的所有元素
std::vector<std::string> RedisService::getAllListItems(const std::string &key)
{
    std::vector<std::string> items;
    try
    {
        if (redis_)
        {
            // 先检查键是否存在和类型是否正确
            if (!keyExists(key))
            {
                LOG_WARN << "Key " << key << " does not exist";
                return items;
            }

            if (!isListType(key))
            {
                LOG_WARN << "Key " << key << " is not a list type";
                return items;
            }

            // 获取列表所有元素
            redis_->lrange(key, 0, -1, std::back_inserter(items));
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Redis getAllListItems error: " << e.what() << " for key: " << key;
    }
    return items;
}

// 获取指定范围的列表元素
std::vector<std::string> RedisService::getListRange(const std::string &key, long long start, long long stop)
{
    std::vector<std::string> items;
    try
    {
        if (redis_)
        {
            redis_->lrange(key, start, stop, std::back_inserter(items));
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Redis getListRange error: " << e.what();
    }
    return items;
}

// 设置键值
bool RedisService::setValue(const std::string &key, const std::string &value)
{
    try
    {
        if (redis_)
        {
            redis_->set(key, value);
            return true;
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Redis setValue error: " << e.what();
    }
    return false;
}

// 获取键值
std::string RedisService::getValue(const std::string &key, const std::string &defaultValue)
{
    try
    {
        if (redis_)
        {
            auto val = redis_->get(key);
            if (val)
            {
                return *val;
            }
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Redis getValue error: " << e.what();
    }
    return defaultValue;
}

// 检查键是否存在
bool RedisService::keyExists(const std::string &key)
{
    try
    {
        if (redis_)
        {
            return redis_->exists(key);
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Redis keyExists error: " << e.what();
    }
    return false;
}

// 检查键是否是列表类型
bool RedisService::isListType(const std::string &key)
{
    try
    {
        if (redis_)
        {
            auto type = redis_->type(key);
            return type == "list";
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Redis isListType error: " << e.what();
    }
    return false;
}

// 获取键的类型
std::string RedisService::getKeyType(const std::string &key)
{
    try
    {
        if (redis_)
        {
            return redis_->type(key);
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Redis getKeyType error: " << e.what();
    }
    return "none";
}

// 修剪列表
bool RedisService::trimList(const std::string &key, long long start, long long stop)
{
    try
    {
        if (redis_)
        {
            redis_->ltrim(key, start, stop);
            return true;
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Redis trimList error: " << e.what();
    }
    return false;
}

// 获取并清除用户离线消息
std::vector<std::string> RedisService::getOfflineMessages(int userId)
{
    std::vector<std::string> messages;
    if (!initialized_ || !redis_)
        return messages;

    try
    {
        // 构建用户离线消息key
        std::string offlineKey = "user:" + std::to_string(userId) + ":offline";

        // 首先获取所有离线消息
        std::vector<std::string> offlineMessages;
        redis_->lrange(offlineKey, 0, -1, std::back_inserter(offlineMessages));

        if (!offlineMessages.empty())
        {
            // 然后删除离线消息键
            redis_->del(offlineKey);
            LOG_INFO << "Retrieved " << offlineMessages.size() << " offline messages for user " << userId;
            return offlineMessages;
        }

        return offlineMessages;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to get offline messages: " << e.what();
        return std::vector<std::string>();
    }
}

// 检查用户是否有离线消息
bool RedisService::hasOfflineMessages(int userId)
{
    if (!initialized_ || !redis_)
        return false;

    try
    {
        // 构建用户离线消息key
        std::string offlineKey = "user:" + std::to_string(userId) + ":offline";

        // 检查列表是否为空
        long long count = redis_->llen(offlineKey);
        return count > 0;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to check offline messages: " << e.what();
        return false;
    }
}

// 获取离线消息计数
int RedisService::getOfflineMessageCount(int userId)
{
    if (!initialized_ || !redis_)
        return 0;

    try
    {
        // 构建用户离线消息key
        std::string offlineKey = "user:" + std::to_string(userId) + ":offline";

        // 获取列表长度
        long long count = redis_->llen(offlineKey);
        return static_cast<int>(count);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Failed to get offline message count: " << e.what();
        return 0;
    }
}
