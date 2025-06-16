#!/usr/bin/env python3
import socket
import time
import threading
import sys

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
        
        # 登录
        login_message = "1:username=testuser;password=password123"
        print(f"发送: {login_message}")
        sock.sendall(login_message.encode())
        
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

def receive_messages(sock):
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
                
                # 如果是登录响应并且成功
                if msg_type == 2:  # LOGIN_RESPONSE
                    content = parts[1].split(";")
                    if any(item.startswith("status=0") for item in content):
                        print("登录成功!")
                
                # 如果是心跳响应
                elif msg_type == 7:  # HEARTBEAT_RESPONSE
                    print("收到心跳响应")
            
        except Exception as e:
            print(f"接收消息错误: {e}")
            break

if __name__ == "__main__":
    main()
