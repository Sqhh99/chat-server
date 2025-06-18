#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import socket
import time
import json

def connect_to_server(port=8888):
    """连接到聊天服务器"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', port))
    return sock

def send_message(sock, message):
    """发送消息到服务器"""
    sock.send(message.encode() + b'\n')

def receive_message(sock, timeout=5):
    """从服务器接收消息"""
    sock.settimeout(timeout)
    try:
        response = sock.recv(1024).decode().strip()
        return response
    except socket.timeout:
        return None

def parse_message_content(content):
    """解析消息内容"""
    data = {}
    if ':' in content:
        parts = content.split(':', 1)
        if len(parts) > 1:
            params = parts[1].split(';')
            for param in params:
                if '=' in param:
                    key, value = param.split('=', 1)
                    data[key] = value
    return data

def test_friend_request_flow():
    """测试好友请求流程"""
    print("=== 好友请求功能测试 ===")
    
    # 连接两个用户
    user1_sock = connect_to_server()
    user2_sock = connect_to_server()
    
    try:
        # 用户1登录
        print("\n1. 用户1登录...")
        send_message(user1_sock, "1:username=testuser1;password=password123")
        response1 = receive_message(user1_sock)
        print(f"用户1登录响应: {response1}")
        
        # 用户2登录
        print("\n2. 用户2登录...")
        send_message(user2_sock, "1:username=testuser2;password=password123")
        response2 = receive_message(user2_sock)
        print(f"用户2登录响应: {response2}")
        
        time.sleep(1)
        
        # 用户1发送好友请求给用户2
        print("\n3. 用户1发送好友请求给用户2...")
        send_message(user1_sock, "28:friendId=testuser2")  # ADD_FRIEND_REQUEST = 28
        response = receive_message(user1_sock)
        print(f"发送好友请求响应: {response}")
        
        # 检查用户2是否收到好友请求通知
        print("\n4. 检查用户2是否收到好友请求通知...")
        notification = receive_message(user2_sock, timeout=3)
        if notification:
            print(f"用户2收到通知: {notification}")
        else:
            print("用户2没有收到通知")
        
        # 用户2获取好友请求列表
        print("\n5. 用户2获取好友请求列表...")
        send_message(user2_sock, "34:")  # GET_FRIEND_REQUESTS = 34
        response = receive_message(user2_sock)
        print(f"好友请求列表响应: {response}")
        
        # 解析好友请求列表，获取用户1的ID
        data = parse_message_content(response)
        if 'requests' in data:
            try:
                requests = json.loads(data['requests'])
                if requests:
                    user1_id = requests[0]['id']
                    print(f"找到好友请求，用户1 ID: {user1_id}")
                    
                    # 用户2接受好友请求
                    print(f"\n6. 用户2接受来自用户{user1_id}的好友请求...")
                    send_message(user2_sock, f"30:fromUserId={user1_id}")  # ACCEPT_FRIEND_REQUEST = 30
                    response = receive_message(user2_sock)
                    print(f"接受好友请求响应: {response}")
                    
                    # 检查用户1是否收到接受通知
                    print("\n7. 检查用户1是否收到接受通知...")
                    notification = receive_message(user1_sock, timeout=3)
                    if notification:
                        print(f"用户1收到通知: {notification}")
                    else:
                        print("用户1没有收到通知")
                        
                    # 验证好友关系
                    print("\n8. 验证好友关系...")
                    
                    # 用户1获取好友列表
                    send_message(user1_sock, "26:")  # GET_USER_FRIENDS = 26
                    response = receive_message(user1_sock)
                    print(f"用户1好友列表: {response}")
                    
                    # 用户2获取好友列表
                    send_message(user2_sock, "26:")  # GET_USER_FRIENDS = 26
                    response = receive_message(user2_sock)
                    print(f"用户2好友列表: {response}")
                    
                else:
                    print("没有找到好友请求")
            except json.JSONDecodeError:
                print("解析好友请求列表失败")
        
        print("\n=== 测试完成 ===")
        
    finally:
        user1_sock.close()
        user2_sock.close()

def test_reject_friend_request():
    """测试拒绝好友请求"""
    print("\n=== 拒绝好友请求测试 ===")
    
    user1_sock = connect_to_server()
    user2_sock = connect_to_server()
    
    try:
        # 用户登录
        send_message(user1_sock, "1:username=testuser1;password=password123")
        receive_message(user1_sock)
        
        send_message(user2_sock, "1:username=testuser3;password=password123")  # 使用不同用户避免冲突
        receive_message(user2_sock)
        
        time.sleep(1)
        
        # 发送好友请求
        print("1. 用户1发送好友请求给用户3...")
        send_message(user1_sock, "28:friendId=testuser3")  # ADD_FRIEND_REQUEST = 28
        response = receive_message(user1_sock)
        print(f"发送好友请求响应: {response}")
        
        # 用户3获取好友请求列表
        print("2. 用户3获取好友请求列表...")
        send_message(user2_sock, "34:")  # GET_FRIEND_REQUESTS = 34
        response = receive_message(user2_sock)
        print(f"好友请求列表: {response}")
        
        # 解析并拒绝请求
        data = parse_message_content(response)
        if 'requests' in data:
            try:
                requests = json.loads(data['requests'])
                if requests:
                    user1_id = requests[0]['id']
                    print(f"3. 用户3拒绝来自用户{user1_id}的好友请求...")
                    send_message(user2_sock, f"32:fromUserId={user1_id}")  # REJECT_FRIEND_REQUEST = 32
                    response = receive_message(user2_sock)
                    print(f"拒绝好友请求响应: {response}")
                    
                    # 验证请求被移除
                    print("4. 验证好友请求被移除...")
                    send_message(user2_sock, "34:")  # GET_FRIEND_REQUESTS = 34
                    response = receive_message(user2_sock)
                    print(f"好友请求列表: {response}")
            except json.JSONDecodeError:
                print("解析失败")
                
    finally:
        user1_sock.close()
        user2_sock.close()

if __name__ == "__main__":
    try:
        test_friend_request_flow()
        time.sleep(2)
        test_reject_friend_request()
    except Exception as e:
        print(f"测试过程中出现错误: {e}")
