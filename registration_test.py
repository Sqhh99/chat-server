#!/usr/bin/env python3
import socket
import time
import threading
import sys
import re

def main():
    server_address = ('localhost', 8888)
    
    print(f"连接到服务器 {server_address[0]}:{server_address[1]}")
    
    # 创建TCP套接字
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    try:
        # 连接服务器
        sock.connect(server_address)
        
        # 创建一个线程来接收服务器消息
        receive_thread = threading.Thread(target=receive_messages, args=(sock,))
        receive_thread.daemon = True  # 设置为守护线程，主线程结束时会被销毁
        receive_thread.start()
        
        # 注册流程
        register_workflow(sock)
        
        # 每20秒发送一次心跳
        while True:
            time.sleep(20)
            heartbeat_message = f"6:timestamp={int(time.time() * 1000)}"
            print(f"发送心跳: {heartbeat_message}")
            sock.sendall(heartbeat_message.encode())
            
    except KeyboardInterrupt:
        print("\n退出程序")
    except Exception as e:
        print(f"错误: {e}")
    finally:
        sock.close()

def register_workflow(sock):
    """用户注册流程"""
    while True:
        # 获取注册信息
        email = input("请输入邮箱: ")
        username = input("请输入用户名: ")
        password = input("请输入密码: ")
        
        # 验证邮箱格式
        if not re.match(r"[^@]+@[^@]+\.[^@]+", email):
            print("邮箱格式不正确，请重新输入")
            continue
            
        # 发送验证码请求
        verify_code_request = f"10:email={email}"
        print(f"发送验证码请求: {verify_code_request}")
        sock.sendall(verify_code_request.encode())
        
        # 等待用户输入验证码
        print("验证码已发送到邮箱，请查收")
        verify_code = input("请输入验证码: ")
        
        # 发送注册请求
        register_message = f"8:email={email};username={username};password={password};code={verify_code}"
        print(f"发送注册请求: {register_message}")
        sock.sendall(register_message.encode())
        
        # 等待接收注册响应
        time.sleep(2)
        user_choice = input("是否继续注册？(y/n): ")
        if user_choice.lower() != 'y':
            break

def receive_messages(sock):
    """接收并处理服务器消息"""
    while True:
        try:
            data = sock.recv(4096)
            if not data:
                print("服务器断开连接")
                break
            
            message = data.decode()
            print(f"接收: {message}")
            
            # 解析消息类型
            parts = message.split(":", 1)
            if len(parts) == 2:
                msg_type = int(parts[0])
                
                # 如果是验证码响应
                if msg_type == 11:  # VERIFY_CODE_RESPONSE
                    content = parts[1].split(";")
                    if any(item.startswith("status=0") for item in content):
                        print("验证码已发送到邮箱，请查收!")
                    else:
                        error_msg = next((item.split("=")[1] for item in content if item.startswith("message=")), "未知错误")
                        print(f"验证码发送失败: {error_msg}")
                
                # 如果是注册响应
                elif msg_type == 9:  # REGISTER_RESPONSE
                    content = parts[1].split(";")
                    if any(item.startswith("status=0") for item in content):
                        print("注册成功!")
                        user_id = next((item.split("=")[1] for item in content if item.startswith("userid=")), "未知")
                        print(f"用户ID: {user_id}")
                    else:
                        error_msg = next((item.split("=")[1] for item in content if item.startswith("message=")), "未知错误")
                        print(f"注册失败: {error_msg}")
                
                # 如果是登录响应
                elif msg_type == 2:  # LOGIN_RESPONSE
                    content = parts[1].split(";")
                    if any(item.startswith("status=0") for item in content):
                        print("登录成功!")
                    else:
                        error_msg = next((item.split("=")[1] for item in content if item.startswith("message=")), "未知错误")
                        print(f"登录失败: {error_msg}")
                
                # 如果是心跳响应
                elif msg_type == 7:  # HEARTBEAT_RESPONSE
                    print("收到心跳响应")
            
        except Exception as e:
            print(f"接收消息错误: {e}")
            break

if __name__ == "__main__":
    main()
