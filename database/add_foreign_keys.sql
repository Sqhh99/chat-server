-- 添加私聊消息表的外键约束
ALTER TABLE private_messages
ADD CONSTRAINT fk_private_messages_from_user
FOREIGN KEY (from_user_id) REFERENCES users(id) ON DELETE CASCADE;

ALTER TABLE private_messages
ADD CONSTRAINT fk_private_messages_to_user
FOREIGN KEY (to_user_id) REFERENCES users(id) ON DELETE CASCADE;

-- 添加群组和创建者的外键约束
ALTER TABLE groups
ADD CONSTRAINT fk_groups_creator
FOREIGN KEY (creator_id) REFERENCES users(id) ON DELETE CASCADE;

-- 添加群组消息的外键约束
ALTER TABLE group_messages
ADD CONSTRAINT fk_group_messages_group
FOREIGN KEY (group_id) REFERENCES groups(id) ON DELETE CASCADE;

ALTER TABLE group_messages
ADD CONSTRAINT fk_group_messages_from_user
FOREIGN KEY (from_user_id) REFERENCES users(id) ON DELETE CASCADE;

-- 添加群组成员的外键约束
ALTER TABLE group_members
ADD CONSTRAINT fk_group_members_group
FOREIGN KEY (group_id) REFERENCES groups(id) ON DELETE CASCADE;

ALTER TABLE group_members
ADD CONSTRAINT fk_group_members_user
FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE;

-- 添加好友关系表的外键约束
ALTER TABLE user_friends
ADD CONSTRAINT fk_user_friends_user1
FOREIGN KEY (user_id1) REFERENCES users(id) ON DELETE CASCADE;

ALTER TABLE user_friends
ADD CONSTRAINT fk_user_friends_user2
FOREIGN KEY (user_id2) REFERENCES users(id) ON DELETE CASCADE;

-- 为了方便数据清理，检查并删除旧的冗余表
-- 请确认这些表不再被使用后再执行以下语句

/*
-- 删除旧表（在确认代码不再依赖它们后执行）
DROP TABLE IF EXISTS messages;
DROP TABLE IF EXISTS chat_members;
DROP TABLE IF EXISTS chats;
*/
