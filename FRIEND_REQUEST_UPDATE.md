# 好友请求功能更新说明

## 概述

原来的添加好友功能存在逻辑问题：用户添加另一个用户为好友时是直接就添加了，不需要另一个用户批准。现在已经修改为需要对方同意的好友请求机制。

## 主要修改

### 1. 消息类型扩展 (ChatServer.h)

添加了新的消息类型来支持好友请求流程：

```cpp
ADD_FRIEND = 28,                // 添加好友 (向后兼容，实际执行好友请求)
ADD_FRIEND_REQUEST = 29,        // 发送好友请求  
ADD_FRIEND_RESPONSE = 30,       // 好友请求响应
ACCEPT_FRIEND_REQUEST = 31,     // 接受好友请求
ACCEPT_FRIEND_RESPONSE = 32,    // 接受好友请求响应
REJECT_FRIEND_REQUEST = 33,     // 拒绝好友请求
REJECT_FRIEND_RESPONSE = 34,    // 拒绝好友请求响应
GET_FRIEND_REQUESTS = 35,       // 获取好友请求列表
FRIEND_REQUESTS_RESPONSE = 36,  // 好友请求列表响应
```

### 2. Redis服务扩展 (RedisService.h/cpp)

添加了好友请求相关的方法：

- `sendFriendRequest(fromUserId, toUserId)` - 发送好友请求
- `acceptFriendRequest(fromUserId, toUserId)` - 接受好友请求
- `rejectFriendRequest(fromUserId, toUserId)` - 拒绝好友请求
- `getFriendRequests(userId)` - 获取用户的好友请求列表
- `hasFriendRequest(fromUserId, toUserId)` - 检查是否已发送好友请求
- `getFriendRequestsKey(userId)` - 生成好友请求的Redis键

### 3. 服务器处理器扩展 (ChatServer.h/cpp)

添加了新的消息处理方法：

- `handleAddFriendRequest()` - 处理发送好友请求
- `handleAcceptFriendRequest()` - 处理接受好友请求  
- `handleRejectFriendRequest()` - 处理拒绝好友请求
- `handleGetFriendRequests()` - 处理获取好友请求列表

### 4. 向后兼容性

原来的 `ADD_FRIEND` (消息类型28) 现在重定向到新的好友请求逻辑，确保现有客户端代码仍然可以工作。

## 好友请求流程

### 发送好友请求
1. 用户A发送 `ADD_FRIEND_REQUEST` 消息给服务器，指定要添加的用户B
2. 服务器验证用户B存在且不是自己
3. 检查是否已经是好友或已发送过请求
4. 将请求添加到用户B的好友请求列表 (Redis: `user:{userId}:friend_requests`)
5. 如果用户B在线，发送实时通知
6. 返回成功响应给用户A

### 处理好友请求
1. 用户B发送 `GET_FRIEND_REQUESTS` 获取待处理的好友请求列表
2. 服务器返回包含请求者信息的JSON列表

### 接受好友请求
1. 用户B发送 `ACCEPT_FRIEND_REQUEST` 消息，指定要接受的请求发送者ID
2. 服务器从好友请求列表中移除该请求
3. 在双方的好友列表中添加对方
4. 如果请求发送者在线，发送接受通知
5. 返回成功响应

### 拒绝好友请求
1. 用户B发送 `REJECT_FRIEND_REQUEST` 消息，指定要拒绝的请求发送者ID
2. 服务器从好友请求列表中移除该请求
3. 不通知请求发送者（避免打扰）
4. 返回成功响应

## Redis数据结构

### 好友请求存储
- 键格式: `user:{userId}:friend_requests`
- 类型: Set
- 内容: 发送好友请求的用户ID集合

### 好友关系存储 (不变)
- 键格式: `user:{userId}:friends`
- 类型: Set
- 内容: 好友用户ID集合

## 客户端使用示例

### 发送好友请求
```
消息格式: "29:friendId=targetUsername"
响应: "30:status=0;friendId=123;username=targetUsername;message=Friend request sent successfully"
```

### 获取好友请求列表
```
消息格式: "35:"
响应: "36:status=0;requests=[{\"id\":123,\"username\":\"sender\",\"online\":true}]"
```

### 接受好友请求
```
消息格式: "31:fromUserId=123"
响应: "32:status=0;fromUserId=123;username=sender;message=Friend request accepted successfully"
```

### 拒绝好友请求
```
消息格式: "33:fromUserId=123"
响应: "34:status=0;fromUserId=123;username=sender;message=Friend request rejected successfully"
```

## 数据库支持

现有的 `user_friends` 表已经包含了 `status` 字段用于支持不同的好友关系状态，当前实现主要使用Redis进行实时处理，通过 `MessageArchiveService` 定期同步到PostgreSQL数据库。

## 测试

提供了 `friend_request_test.py` 脚本来测试完整的好友请求流程，包括：
- 发送好友请求
- 接收好友请求通知
- 获取好友请求列表
- 接受/拒绝好友请求
- 验证好友关系建立

## 安全性和防护

1. **重复请求防护**: 检查是否已发送过好友请求
2. **自己添加自己防护**: 禁止用户添加自己为好友
3. **已存在好友防护**: 检查是否已经是好友关系
4. **用户存在性验证**: 验证目标用户是否存在
5. **权限验证**: 只有登录用户才能执行好友相关操作

这次修改完全解决了原来直接添加好友的问题，现在需要对方明确同意才能建立好友关系，符合常见社交应用的用户体验。
