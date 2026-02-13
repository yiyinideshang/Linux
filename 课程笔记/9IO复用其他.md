# I/O模式对比

| 模式            | 描述                           | 适用场景       |
| --------------- | ------------------------------ | -------------- |
| **阻塞I/O**     | 调用后一直等待直到数据就绪     | 简单应用       |
| **非阻塞I/O**   | 立即返回，需要轮询检查状态     | 低并发场景     |
| **I/O复用**     | 监控多个描述符，任一就绪即处理 | 高并发网络服务 |
| **信号驱动I/O** | 内核通过信号通知数据就绪       | 较少使用       |
| **异步I/O**     | 整个操作完成后才通知           | 高性能应用     |

# 4. **kqueue（BSD/MacOS）**

类似于epoll，用于BSD系统

# 5. **IOCP（Windows）**

Windows的异步I/O完成端口

# 各种实现技术 总结对比

| 场景                | 推荐方案              |
| :------------------ | :-------------------- |
| 少量连接(<1000)     | select/poll           |
| 大量连接，Linux系统 | epoll                 |
| 跨平台需求          | poll + 条件编译       |
| 极高并发(>10K)      | epoll + 多线程/多进程 |
| 需要精确超时控制    | timerfd + epoll       |

# 实际应用示例

```python
# 使用select的简单echo服务器
import select
import socket

server = socket.socket()
server.bind(('0.0.0.0', 8888))
server.listen(5)

inputs = [server]  # 监控的socket列表

while True:
    # select监控所有socket
    readable, writable, exceptional = select.select(inputs, [], [])
    
    for s in readable:
        if s is server:  # 有新连接
            conn, addr = s.accept()
            inputs.append(conn)  # 加入监控列表
        else:  # 客户端socket有数据
            data = s.recv(1024)
            if data:
                s.send(data)  # 回显数据
            else:
                inputs.remove(s)  # 连接关闭
                s.close()
```

# 主要应用场景

1. **网络服务器**：Web服务器、聊天服务器、代理服务器
2. **实时通信系统**：需要同时处理多个客户端连接
3. **文件传输工具**：同时监控多个文件传输进度
4. **数据库连接池**：管理多个数据库连接

# 优势

- **资源高效**：单线程处理多连接，减少上下文切换
- **编程简洁**：避免多线程/进程的同步问题
- **可扩展性好**：适合高并发场景

# 缺点

- **编程复杂度**：比传统的阻塞I/O复杂
- **调试困难**：事件驱动模式调试较困难
- **不适合CPU密集型任务**：长时计算会阻塞事件循环

I/O复用是现代高性能网络编程的基石技术，理解它对于开发高并发服务器至关重要。