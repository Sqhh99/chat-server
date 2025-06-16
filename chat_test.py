#!/usr/bin/env python3
import socket
import threading
import time
import json
import sys

def send_message(sock, msg_type, data):
    """
    发送消息到服务器
    :param sock: 套接字
    :param msg_type: 消息类型
    :param data: 消息数据字典
    """
    # 构建消息字符串: msgType:key1=value1;key2=value2;...
    data_str = ";".join([f"{key}={value}" for key, value in data.items()])
    message = f"{msg_type}:{data_str}\n"
    
    print(f"发送: {message.strip()}")
    sock.sendall(message.encode())

def receive_messages(sock, user_id=None):
    """接收服务器消息"""
    while True:
        try:
            data = sock.recv(4096)
            if not data:
                print("\n服务器断开连接")
                break
            
            # 处理接收到的消息
            messages = data.decode().split('\n')
            for msg in messages:
                if not msg:
                    continue
                    
                print(f"\n收到: {msg}")
                
                # 解析消息
                parts = msg.split(':', 1)
                if len(parts) == 2:
                    msg_type = int(parts[0])
                    content = parts[1]
                    
                    # 处理不同类型的消息
                    if msg_type == 12:  # PRIVATE_CHAT
                        read_info = handle_private_chat(content)
                        # 自动标记消息已读
                        if read_info:
                            send_message(sock, 34, read_info)
                    elif msg_type == 13:  # GROUP_CHAT
                        read_info = handle_group_chat(content)
                        # 自动标记消息已读
                        if read_info:
                            send_message(sock, 34, read_info)
                    elif msg_type == 7:  # HEARTBEAT_RESPONSE
                        # 忽略心跳响应的打印
                        pass
                    elif msg_type == 33:  # RECALL_MESSAGE_RESPONSE
                        data = parse_message_content(content)
                        msg_type = data.get('type', '')
                        if msg_type == 'private':
                            print(f"\n对方撤回了一条消息")
                        elif msg_type == 'group':
                            print(f"\n群 {data.get('groupId', '未知群组')} 中有人撤回了一条消息")
                    elif msg_type == 35:  # MARK_MESSAGE_READ_RESPONSE
                        data = parse_message_content(content)
                        print(f"\n消息已被对方阅读")
                    elif msg_type == 31:  # CHAT_HISTORY_RESPONSE
                        handle_chat_history(content)
                    elif msg_type == 23:  # GROUP_LIST_RESPONSE 
                        handle_group_list_response(content)
                    elif msg_type == 25:  # GROUP_MEMBERS_RESPONSE
                        handle_group_members_response(content)
                    elif msg_type == 21:  # USER_LIST_RESPONSE 
                        handle_user_list_response(content)
                    elif msg_type == 27:  # USER_FRIENDS_RESPONSE
                        handle_user_friends_response(content)
                    elif msg_type == 29:  # ADD_FRIEND_RESPONSE
                        handle_add_friend_response(content)
                    elif msg_type == 2:  # LOGIN_RESPONSE
                        data = parse_message_content(content)
                        if data.get('status') == '0':  # 登录成功
                            user_id = data.get('userId')
                            offline_count = data.get('offlineMsgCount')
                            if offline_count:
                                print(f"\n您有 {offline_count} 条未读消息，正在加载...")
                    
        except Exception as e:
            print(f"\n接收消息出错: {e}")
            break

def parse_message_content(content):
    """解析消息内容为字典"""
    data = {}
    for item in content.split(';'):
        if '=' in item:
            key, value = item.split('=', 1)
            data[key] = value
    return data

def handle_private_chat(content):
    """处理私聊消息"""
    data = parse_message_content(content)
    from_user = data.get('fromUsername', data.get('fromUserId', '未知用户'))
    message_id = data.get('messageId', '')
    
    # 检查消息是否被撤回
    if data.get('recalled') == 'true':
        print(f"\n[私聊] {from_user} 撤回了一条消息")
    else:
        print(f"\n[私聊] {from_user}: {data.get('content', '')}")
        
    # 如果有消息ID，自动标记为已读
    if message_id:
        return {
            'type': 'private',
            'messageId': message_id,
            'fromUserId': data.get('fromUserId', '')
        }
    return None

def handle_group_chat(content):
    """处理群聊消息"""
    data = parse_message_content(content)
    from_user = data.get('fromUsername', data.get('fromUserId', '未知用户'))
    group_id = data.get('groupId', '未知群组')
    message_id = data.get('messageId', '')
    
    # 检查消息是否被撤回
    if data.get('recalled') == 'true':
        print(f"\n[群聊][{group_id}] {from_user} 撤回了一条消息")
    else:
        print(f"\n[群聊][{group_id}] {from_user}: {data.get('content', '')}")
    
    # 如果有消息ID，自动标记为已读
    if message_id and group_id:
        return {
            'type': 'group',
            'messageId': message_id,
            'groupId': group_id
        }
    return None

def handle_chat_history(content):
    """处理聊天历史记录"""
    data = parse_message_content(content)
    history_type = data.get('type', '')
    messages_json = data.get('messages', '[]')
    
    try:
        messages = json.loads(messages_json)
        
        if history_type == 'private':
            user_id = data.get('userId', '')
            target_id = data.get('targetId', '')
            print(f"\n===== 与用户 {target_id} 的聊天历史 =====")
        elif history_type == 'group':
            group_id = data.get('groupId', '')
            print(f"\n===== 群组 {group_id} 的聊天历史 =====")
        
        # 按时间排序消息（从旧到新）
        messages.sort(key=lambda x: int(x.get('timestamp', 0)))
        
        for msg in messages:
            from_id = msg.get('from', '')
            content = msg.get('content', '')
            timestamp = int(msg.get('timestamp', 0)) / 1000  # 转换为秒
            time_str = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(timestamp))
            
            if history_type == 'private':
                print(f"[{time_str}] 用户 {from_id}: {content}")
            elif history_type == 'group':
                print(f"[{time_str}] 用户 {from_id}: {content}")
        
        if not messages:
            print("暂无聊天记录")
        print("=" * 40)
    except json.JSONDecodeError as e:
        print(f"\n解析聊天历史记录失败: {e}")
    except Exception as e:
        print(f"\n处理聊天历史记录时出错: {e}")

def handle_group_list_response(content):
    """处理群组列表响应"""
    data = parse_message_content(content)
    status = data.get('status')
    if status == '0':
        groups_json = data.get('groups', '[]')
        try:
            # 尝试解析JSON字符串
            groups = json.loads(groups_json)
            print("\n===== 您的群组列表 =====")
            for group in groups:
                print(f"ID: {group.get('id', 'N/A')}, 名称: {group.get('name', 'Unknown')}")
            print("=======================")
        except json.JSONDecodeError as e:
            print(f"\n解析群组列表失败: {e}")
            print(f"原始数据: {groups_json}")
    else:
        print("\n获取群组列表失败")

def handle_group_members_response(content):
    """处理群组成员列表响应"""
    data = parse_message_content(content)
    status = data.get('status')
    if status == '0':
        group_id = data.get('groupId', 'Unknown')
        members_json = data.get('members', '[]')
        try:
            # 尝试解析JSON字符串
            members = json.loads(members_json)
            print(f"\n===== 群组 {group_id} 的成员 =====")
            for member in members:
                online_status = "在线" if member.get('online', False) else "离线"
                print(f"ID: {member.get('id', 'N/A')}, 用户名: {member.get('username', 'Unknown')}, 状态: {online_status}")
            print("=========================")
        except json.JSONDecodeError as e:
            print(f"\n解析群组成员列表失败: {e}")
            print(f"原始数据: {members_json}")
    else:
        print(f"\n获取群组成员列表失败: {data.get('message', '未知错误')}")

def handle_user_list_response(content):
    """处理在线用户列表响应"""
    data = parse_message_content(content)
    status = data.get('status')
    if status == '0':
        users_json = data.get('users', '[]')
        try:
            # 尝试解析JSON字符串
            users = json.loads(users_json)
            print("\n===== 在线用户列表 =====")
            for user in users:
                print(f"ID: {user.get('id', 'N/A')}, 用户名: {user.get('username', 'Unknown')}")
            print("=======================")
        except json.JSONDecodeError as e:
            print(f"\n解析用户列表失败: {e}")
            print(f"原始数据: {users_json}")
    else:
        print(f"\n获取用户列表失败: {data.get('message', '未知错误')}")

def handle_user_friends_response(content):
    """处理好友列表响应"""
    data = parse_message_content(content)
    status = data.get('status')
    if status == '0':
        friends_json = data.get('friends', '[]')
        try:
            # 尝试解析JSON字符串
            friends = json.loads(friends_json)
            print("\n===== 好友列表 =====")
            for friend in friends:
                online_status = "在线" if friend.get('online', False) else "离线"
                print(f"ID: {friend.get('id', 'N/A')}, 用户名: {friend.get('username', 'Unknown')}, 状态: {online_status}")
            print("====================")
        except json.JSONDecodeError as e:
            print(f"\n解析好友列表失败: {e}")
            print(f"原始数据: {friends_json}")
    else:
        print(f"\n获取好友列表失败: {data.get('message', '未知错误')}")

def handle_add_friend_response(content):
    """处理添加好友响应"""
    data = parse_message_content(content)
    status = data.get('status')
    if status == '0':
        friend_id = data.get('friendId', 'Unknown')
        username = data.get('username', 'Unknown')
        print(f"\n✅ 成功添加好友: ID: {friend_id}, 用户名: {username}")
    else:
        print(f"\n❌ 添加好友失败: {data.get('message', '未知错误')}")

def main():
    # 服务器地址
    server_address = ('localhost', 8888)
    
    print(f"连接到服务器 {server_address[0]}:{server_address[1]}")
    
    # 创建TCP套接字
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    try:
        # 连接服务器
        sock.connect(server_address)
        
        # 创建接收线程
        receive_thread = threading.Thread(target=receive_messages, args=(sock,))
        receive_thread.daemon = True
        receive_thread.start()
        
        # 用户登录
        username = input("用户名: ")
        password = input("密码: ")
        
        # 发送登录请求
        send_message(sock, 1, {"username": username, "password": password})
        
        # 等待登录响应
        time.sleep(1)
        
        user_id = None
        logged_in = True
        
        # 主循环
        while logged_in:
            print("\n===== 聊天菜单 =====")
            print("1. 发送私聊消息")
            print("2. 发送群聊消息")
            print("3. 创建群组")
            print("4. 加入群组")
            print("5. 离开群组")
            print("6. 获取在线用户列表")
            print("7. 获取群组列表")
            print("8. 获取群组成员")
            print("9. 撤回消息")
            print("10. 获取聊天历史记录")
            print("11. 获取好友列表")
            print("12. 添加好友")
            print("13. 退出")
            
            choice = input("\n选择功能: ")
            
            if choice == "1":
                # 私聊
                to_user_id = input("接收者用户ID: ")
                content = input("消息内容: ")
                send_message(sock, 12, {"toUserId": to_user_id, "content": content})
                
            elif choice == "2":
                # 群聊
                group_id = input("群组ID (请输入数字): ")
                content = input("消息内容: ")
                send_message(sock, 13, {"groupId": group_id, "content": content})
                
            elif choice == "3":
                # 创建群组
                group_name = input("群组名称: ")
                send_message(sock, 14, {"groupName": group_name})
                
            elif choice == "4":
                # 加入群组
                group_id = input("群组ID (请输入数字): ")
                send_message(sock, 16, {"groupId": group_id})
                
            elif choice == "5":
                # 离开群组
                group_id = input("群组ID (请输入数字): ")
                send_message(sock, 18, {"groupId": group_id})
                
            elif choice == "6":
                # 获取在线用户列表
                send_message(sock, 20, {})
                
            elif choice == "7":
                # 获取群组列表
                send_message(sock, 22, {})
                
            elif choice == "8":
                # 获取群组成员
                group_id = input("群组ID (请输入数字): ")
                send_message(sock, 24, {"groupId": group_id})
                
            elif choice == "9":
                # 撤回消息
                message_type = input("消息类型 (private/group): ")
                message_id = input("消息ID: ")
                
                if message_type == "private":
                    target_user_id = input("对方用户ID: ")
                    send_message(sock, 32, {"messageId": message_id, "type": "private", "targetUserId": target_user_id})
                elif message_type == "group":
                    group_id = input("群组ID: ")
                    send_message(sock, 32, {"messageId": message_id, "type": "group", "groupId": group_id})
                else:
                    print("无效的消息类型!")
                    
            elif choice == "10":
                # 获取聊天历史记录
                chat_type = input("聊天类型 (private/group): ")
                
                if chat_type == "private":
                    other_user_id = input("对方用户ID: ")
                    count = input("获取消息数量 (默认20): ") or "20"
                    send_message(sock, 30, {"type": "private", "targetUserId": other_user_id, "count": count})
                elif chat_type == "group":
                    group_id = input("群组ID: ")
                    count = input("获取消息数量 (默认20): ") or "20"
                    send_message(sock, 30, {"type": "group", "groupId": group_id, "count": count})
                else:
                    print("无效的聊天类型!")
                    
            elif choice == "11":
                # 获取好友列表
                send_message(sock, 26, {})
                
            elif choice == "12":
                # 添加好友
                friend_id = input("输入好友用户名或ID: ")
                send_message(sock, 28, {"friendId": friend_id})
                
            elif choice == "13":
                # 退出
                print("退出聊天...")
                if user_id:
                    send_message(sock, 3, {"userId": user_id})
                logged_in = False
            
            else:
                print("无效选择，请重试!")
            
            # 发送心跳
            if logged_in:
                time.sleep(1)  # 让响应有时间显示
                send_message(sock, 6, {"timestamp": int(time.time() * 1000)})
    
    except KeyboardInterrupt:
        print("\n程序中断")
    except Exception as e:
        print(f"错误: {e}")
    finally:
        sock.close()

if __name__ == "__main__":
    main()
