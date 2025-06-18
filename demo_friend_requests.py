#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import socket
import time
import json

def connect_to_server():
    """连接到聊天服务器"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', 8888))
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

def demo_friend_requests():
    """演示好友请求功能"""
    print("🚀 好友请求功能演示")
    print("=" * 50)
    
    # 连接用户alice和bob
    alice_sock = connect_to_server()
    bob_sock = connect_to_server()
    
    try:
        print("\n📍 步骤1: 用户登录")
        # alice登录
        send_message(alice_sock, "1:username=alice;password=password123")
        alice_response = receive_message(alice_sock)
        print(f"Alice登录: {alice_response}")
        
        # bob登录  
        send_message(bob_sock, "1:username=bob;password=password123")
        bob_response = receive_message(bob_sock)
        print(f"Bob登录: {bob_response}")
        
        time.sleep(1)
        
        print("\n📍 步骤2: Alice发送好友请求给Bob")
        send_message(alice_sock, "28:friendId=bob")
        response = receive_message(alice_sock)
        print(f"发送好友请求结果: {response}")
        
        # 检查Bob是否收到通知
        notification = receive_message(bob_sock, timeout=2)
        if notification:
            print(f"Bob收到通知: {notification}")
        
        print("\n📍 步骤3: Bob查看好友请求列表")
        send_message(bob_sock, "34:")
        response = receive_message(bob_sock)
        print(f"Bob的好友请求列表: {response}")
        
        print("\n📍 步骤4: Bob接受Alice的好友请求")
        send_message(bob_sock, "30:fromUserId=7")  # alice的用户ID是7
        response = receive_message(bob_sock)
        print(f"接受好友请求结果: {response}")
        
        # 检查Alice是否收到通知
        notification = receive_message(alice_sock, timeout=2)
        if notification:
            print(f"Alice收到通知: {notification}")
        
        print("\n📍 步骤5: 验证好友关系")
        # Alice查看好友列表
        send_message(alice_sock, "26:")
        response = receive_message(alice_sock)
        print(f"Alice的好友列表: {response}")
        
        # Bob查看好友列表
        send_message(bob_sock, "26:")
        response = receive_message(bob_sock)
        print(f"Bob的好友列表: {response}")
        
        print("\n✅ 好友请求功能演示完成!")
        
    except Exception as e:
        print(f"❌ 演示过程中发生错误: {e}")
    
    finally:
        alice_sock.close()
        bob_sock.close()

if __name__ == "__main__":
    demo_friend_requests()
