-- 创建测试用户的SQL脚本
-- 注意：密码使用简单的明文，实际应用中应该使用哈希加密

-- 创建 testuser1
INSERT INTO users (username, email, password, verified) 
VALUES ('testuser1', 'testuser1@example.com', 'password123', true)
ON CONFLICT (username) DO NOTHING;

-- 创建 testuser2  
INSERT INTO users (username, email, password, verified) 
VALUES ('testuser2', 'testuser2@example.com', 'password123', true)
ON CONFLICT (username) DO NOTHING;

-- 创建 testuser3
INSERT INTO users (username, email, password, verified) 
VALUES ('testuser3', 'testuser3@example.com', 'password123', true)
ON CONFLICT (username) DO NOTHING;

-- 创建 alice
INSERT INTO users (username, email, password, verified) 
VALUES ('alice', 'alice@example.com', 'password123', true)
ON CONFLICT (username) DO NOTHING;

-- 创建 bob
INSERT INTO users (username, email, password, verified) 
VALUES ('bob', 'bob@example.com', 'password123', true)
ON CONFLICT (username) DO NOTHING;

-- 查看创建的用户
SELECT id, username, email, verified, create_time FROM users 
WHERE username IN ('testuser1', 'testuser2', 'testuser3', 'alice', 'bob')
ORDER BY id;
