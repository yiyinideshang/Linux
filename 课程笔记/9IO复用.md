# I/O复用（I/O Multiplexing）

**I/O复用**是一种使单个进程/线程能够同时监控多个I/O文件描述符（如socket、文件、管道等）的机制，当其中任意一个描述符就绪（可读、可写或出现异常）时，程序就能及时处理，从而实现在单线程中处理多个I/O操作。

## 核心思想
- **一个监控者管理多个I/O通道**
- **避免阻塞等待**：不让进程因等待某个I/O而阻塞，而是同时监控所有关心的I/O
- **事件驱动**：当有I/O事件发生时才进行处理

# 主要实现技术

# 9.1. **select**
最古老的I/O复用机制
```c
// 示例：监控多个socket
fd_set read_fds;
FD_ZERO(&read_fds);
FD_SET(sock1, &read_fds);
FD_SET(sock2, &read_fds);

select(max_fd + 1, &read_fds, NULL, NULL, NULL);
```
**缺点**：

- **文件描述符数量有限制**（通常1024）
- 每次调用需要复制整个描述符集合
- 线性扫描所有描述符，效率低

## fd_set结构体

- 本质：一个**位掩码数组**，每一位代表一个文件描述符。
- 具体实现随系统不同而变，但程序员**不需要关心内部细节**，只能通过以下四个标准宏来操作：
  - `FD_ZERO()`
  - `FD_SET()`
  - `FD_CLR()`
  - `FD_ISSET()`
- 限制：传统 `fd_set` 能容纳的文件描述符数量由常量 `FD_SETSIZE` 决定（通常是 1024）。现代系统可能提供更大的或动态的替代方案（如 `poll`、`epoll`），但 `fd_set` 依然在兼容代码中广泛使用。

## getsockopt

`getsockopt` 是 **获取套接字选项当前值** 的系统调用。它允许应用程序查询某个套接字在指定协议层上特定选项的设置，常用于检查套接字状态、缓冲区大小、超时时间、错误信息等。

- 函数原型

```c
#include <sys/socket.h>

int getsockopt(int sockfd, int level, int optname,
               void *optval, socklen_t *optlen);
```

| 参数      | 说明                                                         |
| :-------- | :----------------------------------------------------------- |
| `sockfd`  | 套接字文件描述符。                                           |
| `level`   | 选项定义的协议层： • `SOL_SOCKET`：通用套接字层 • `IPPROTO_TCP`：TCP 协议层 • `IPPROTO_IP`：IPv4 协议层 • `IPPROTO_IPV6`：IPv6 协议层 |
| `optname` | 需要获取的选项名称（如 `SO_RCVBUF`、`SO_ERROR`）。           |
| `optval`  | 指向用于存放选项值的缓冲区。                                 |
| `optlen`  | **值-结果参数**： • 调用前：必须初始化为 `optval` 指向的缓冲区大小（字节数）。 • 返回后：被内核修改为实际写入 `optval` 的数据长度。 |

- 常用选项

### 1. 通用套接字层（`SOL_SOCKET`）

| 选项名         | 说明                                     | 数据类型 |
| :------------- | :--------------------------------------- | :------- |
| `SO_ERROR`     | 获取套接字待处理的错误，读取后自动清除。 | `int`    |
| `SO_RCVBUF`    | 接收缓冲区大小（字节）。                 | `int`    |
| `SO_SNDBUF`    | 发送缓冲区大小（字节）。                 | `int`    |
| `SO_REUSEADDR` | 地址重用标志（1/0）。                    | `int`    |
| `SO_KEEPALIVE` | TCP 保活标志（1/0）。                    | `int`    |
| `SO_TYPE`      | 套接字类型（如 `SOCK_STREAM`）。         | `int`    |

### 2. TCP 协议层（`IPPROTO_TCP`）

| 选项名        | 说明                      | 数据类型 |
| :------------ | :------------------------ | :------- |
| `TCP_NODELAY` | 禁用 Nagle 算法（1/0）。  | `int`    |
| `TCP_MAXSEG`  | TCP 最大分段大小（MSS）。 | `int`    |
| `TCP_CORK`    | 禁止发送部分帧（Linux）。 | `int`    |

- ## 返回值

  - **成功**：返回 `0`。
  - **失败**：返回 `-1`，并设置 `errno` 指示错误（常见错误：`EBADF`、`ENOPROTOOPT`、`EFAULT` 等）。

### 常用示例

- ### 获取接收缓冲区大小

```c
int sockfd;
int rcvbuf;
socklen_t optlen = sizeof(rcvbuf);

if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &optlen) == 0) {
    printf("接收缓冲区大小: %d 字节\n", rcvbuf);
}
```

- ### 检查并清除挂起的套接字错误,将错误码转移到 `error` 并清除套接字错误。

```c
int error;
socklen_t errlen = sizeof(error);

if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &errlen) == 0) {
    if (error != 0) {
        fprintf(stderr, "套接字挂起错误: %s\n", strerror(error));
        // 此时错误已被清除
    }
}
```

### 注意事项

1. **缓冲区大小必须正确传递**：`optlen` 初始值必须 >= 选项真实数据大小，否则可能截断或返回错误。
2. **`SO_ERROR` 的特殊性**：获取后内核会自动清除该错误，再次查询会返回 0。
3. **线程安全**：`getsockopt` 是线程安全的，但读取共享套接字时需自行同步。
4. **平台差异**：某些选项是 Linux 特有（如 `TCP_CORK`），跨平台代码需注意兼容性。

## 与 `setsockopt` 的关系

- **`getsockopt`**：读取当前值。
- **`setsockopt`**：修改选项值。
- 两者共享相同的 `level` 和 `optname` 命名，参数结构几乎一致，仅 `getsockopt` 的 `optlen` 是 **值-结果参数**，而 `setsockopt` 的 `optlen` 仅用于输入选项长度。

# 9.2. **poll**

`poll`是Unix/Linux系统中用于I/O多路复用的系统调用，允许程序同时监视多个文件描述符，等待其中任意一个变为可读、可写或发生异常。

## 函数原型

```c++
#include <poll.h>

int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```

- ### 1. `struct pollfd *fds`

这是一个指向`pollfd`结构体数组的指针：

```c++
struct pollfd {
    int fd;         /* 文件描述符 */
    short events;   /* 请求的事件（监视什么） */
    short revents;  /* 返回的事件（实际发生了什么） */
};
```

**events字段（输入）**：

- `POLLIN`：数据可读
- `POLLOUT`：数据可写
- `POLLPRI`：紧急数据可读（带外数据）
- `POLLERR`：错误条件发生
- `POLLHUP`：挂起（连接断开）
- `POLLNVAL`：无效请求（fd未打开）

**revents字段（输出）**：

- 返回实际发生的事件，可能包含`events`中的标志位，也可能包含错误标志

- ## 2.  `nfds_t nfds`

要监视的文件描述符数量，即`fds`数组的元素个数。

- ## 3. `int timeout`

超时时间（毫秒）：

- `-1`：阻塞等待，直到有事件发生
- `0`：立即返回，不阻塞
- `>0`：等待指定的毫秒数

- ## 返回值

- `>0`：有事件发生的文件描述符数量
- `0`：超时，没有事件发生
- `-1`：发生错误，`errno`被设置

## 与select对比

| 特性           | poll     | select                 |
| :------------- | :------- | :--------------------- |
| 最大文件描述符 | 无限制   | 有限制（FD_SETSIZE）   |
| 效率           | O(n)扫描 | O(n)扫描               |
| 重新初始化     | 需要     | 需要                   |
| 可移植性       | 较好     | 很好                   |
| 超时精度       | 毫秒     | 微秒（struct timeval） |

## 基本使用示例

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>

int main() {
    struct pollfd fds[2];
    int timeout = 5000; // 5秒超时
    char buffer[256];
    
    // 监视标准输入
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    
    // 创建一个管道作为示例
    int pipefd[2];
    pipe(pipefd);
    write(pipefd[1], "Hello", 5);
    
    // 监视管道读取端
    fds[1].fd = pipefd[0];
    fds[1].events = POLLIN;
    
    printf("等待输入或管道数据（5秒超时）...\n");
    
    int ret = poll(fds, 2, timeout);
    
    if (ret == -1) {
        perror("poll");
        return 1;
    } else if (ret == 0) {
        printf("超时！\n");
    } else {
        // 检查哪些文件描述符有事件
        for (int i = 0; i < 2; i++) {
            if (fds[i].revents & POLLIN) {
                if (i == 0) {
                    printf("标准输入可读\n");
                    fgets(buffer, sizeof(buffer), stdin);
                    printf("输入内容: %s", buffer);
                } else {
                    printf("管道可读\n");
                    read(pipefd[0], buffer, sizeof(buffer));
                    printf("管道数据: %s\n", buffer);
                }
            }
            
            if (fds[i].revents & POLLERR) {
                printf("文件描述符 %d 发生错误\n", fds[i].fd);
            }
            
            if (fds[i].revents & POLLHUP) {
                printf("文件描述符 %d 挂起\n", fds[i].fd);
            }
        }
    }
    
    close(pipefd[0]);
    close(pipefd[1]);
    return 0;
}
```
## 服务器端应用示例

```c++
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    struct pollfd fds[MAX_CLIENTS + 1];
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];
    
    // 创建服务器socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);
    
    // 绑定socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    // 监听
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("服务器启动，监听端口 8080...\n");
    
    // 初始化pollfd数组
    for (int i = 0; i <= MAX_CLIENTS; i++) {
        fds[i].fd = -1;
        fds[i].events = POLLIN;
    }
    
    // 添加服务器socket到poll
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    
    while (1) {
        int ret = poll(fds, MAX_CLIENTS + 1, -1); // 阻塞等待
        
        if (ret < 0) {
            perror("poll");
            break;
        }
        
        // 检查服务器socket是否有新连接
        if (fds[0].revents & POLLIN) {
            if ((new_socket = accept(server_fd, 
                                     (struct sockaddr *)&address, 
                                     (socklen_t*)&addrlen)) < 0) {
                perror("accept");
                continue;
            }
            
            printf("新客户端连接: %s:%d\n", 
                   inet_ntoa(address.sin_addr), 
                   ntohs(address.sin_port));
            
            // 将新socket添加到poll
            for (int i = 1; i <= MAX_CLIENTS; i++) {
                if (fds[i].fd == -1) {
                    fds[i].fd = new_socket;
                    fds[i].events = POLLIN;
                    break;
                }
            }
        }
        
        // 检查客户端socket
        for (int i = 1; i <= MAX_CLIENTS; i++) {
            if (fds[i].fd == -1) continue;
            
            if (fds[i].revents & POLLIN) {
                int valread = read(fds[i].fd, buffer, BUFFER_SIZE);
                if (valread <= 0) {
                    // 连接关闭或出错
                    printf("客户端断开连接\n");
                    close(fds[i].fd);
                    fds[i].fd = -1;
                } else {
                    buffer[valread] = '\0';
                    printf("收到消息: %s", buffer);
                    
                    // 回显消息
                    send(fds[i].fd, buffer, valread, 0);
                }
            }
        }
    }
    
    close(server_fd);
    return 0;
}
```

## 优点和缺点

- ## 优点

1. **无文件描述符数量限制**：不像select有FD_SETSIZE限制
2. **更高效的超时处理**：使用毫秒，而非select的微秒结构
3. **更直观的事件处理**：分开的events和revents字段

- ## 缺点

1. **可移植性**：不如select广泛支持
2. **性能问题**：当监视大量文件描述符时，**仍需线性扫描**
3. **需要用户管理数据结构**：需要自己维护pollfd数组

## 最佳实践建议

1. **合理设置超时**：避免永久阻塞，使用合理的超时时间
2. **动态管理pollfd数组**：根据需要动态调整数组大小
3. **错误处理**：总是检查POLLERR、POLLHUP、POLLNVAL
4. **性能考虑**：对于大量连接，考虑使用epoll（Linux）或kqueue（BSD）
5. **资源清理**：及时关闭不再需要的文件描述符



# 9.3. **epoll（Linux特有）**

`epoll`是Linux特有的高性能I/O多路复用机制，专为处理大量并发连接而设计。它解决了select/poll在大量文件描述符时性能下降的问题。

epoll在实现上与select和poll有很大差异，它使用一组函数来完成而不是单个参数。

epoll将用户关心的文件描述符上的事件放在内核里的一个事件表中，无须像 select和poll那要那个每次调用都要重复传入文件描述符集或时间集。它需要一个额外的文件描述符来 唯一表示 内核中的这个**事件表**

## 三种工作模式

1. **水平触发（Level-Triggered，LT）**：默认模式，类似poll的行为
2. **边缘触发（Edge-Triggered，ET）**：只在状态变化时通知
3. **一次性触发（EPOLLONESHOT）**：事件只触发一次，需重新注册

## 主要API

### 1. epoll_create / epoll_create1

创建一个文件描述符来表示上述的那个事件表

```c++
#include <sys/epoll.h>

int epoll_create(int size);  // size参数已废弃，但必须大于0
int epoll_create1(int flags); // flags: 0 或 EPOLL_CLOEXEC
```

- 创建epoll实例，返回epoll文件描述符,将其作为其他所有epoll系统调用哦的第一个参数，用来指定要访问的内核事件表。
- `epoll_create1(0)`等同于`epoll_create(1)`

### 2. epoll_ctl

```c
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
```

**操作类型（op）：**

- `EPOLL_CTL_ADD`：添加新的fd到epoll实例
- `EPOLL_CTL_MOD`：修改已注册的fd
- `EPOLL_CTL_DEL`：从epoll实例中删除fd

**fd参数：要操作的文件描述符**

**epoll_event结构：**

```c++
struct epoll_event {
    uint32_t events;      /* Epoll events epoll事件*/
    epoll_data_t data;    /* User data variable 用户数据 */
};

typedef union epoll_data {
    void    *ptr;//指定与fd相关的用户数据
    int      fd;//指定事件所从属的目标文件描述符
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;
```

**events标志：**

- `EPOLLIN`：可读事件
- `EPOLLOUT`：可写事件
- `EPOLLPRI`：紧急数据可读
- `EPOLLERR`：错误事件
- `EPOLLHUP`：挂起事件
- `EPOLLET`：**边缘触发模式**
- `EPOLLONESHOT`：**一次性事件**
- `EPOLLRDHUP`：对端关闭连接或半关闭

### 3. epoll_wait

epoll系列系统调用的主要接口。在一段超时时间内等待一组文件描述符上的事件。

```c++
int epoll_wait(int epfd, struct epoll_event *events,
               int maxevents, int timeout);
```

- 等待事件发生
- `timeout`：-1阻塞，0立即返回，>0毫秒数；与poll接口的timeout参数相同
- `maxevents`：指定最多监听多少个事件，必须大于0。
- `events`：这个函数一旦检测到事件，就将所有就绪的事件从 内核事件表(epfd指定的那个)中 复制到第二个参数 events指向的数组中。 这个数组只用于输出epoll_wait检测到的就绪事件，而不像select和poll的数组参数那样既用于传入用户注册 事件，又用于输出内核检测到的就绪行事件。这样极大地提高了应用程序索引就绪文件描述符的效率

- 成功时返回就绪的文件描述符的个数，失败时返回-1，并设置errno

## 核心优势

### 与select/poll对比

| 特性         | select             | poll             | epoll             |
| :----------- | :----------------- | :--------------- | :---------------- |
| 最大描述符数 | FD_SETSIZE(1024)   | 无限制           | 无限制            |
| 时间复杂度   | O(n)               | O(n)             | O(1)              |
| 事件通知     | 主动轮询           | 主动轮询         | 回调通知          |
| 内存使用     | 每次复制整个fd_set | 每次复制整个数组 | 内核维护就绪列表  |
| 触发模式     | 水平触发           | 水平触发         | 支持水平/边缘触发 |

### 性能优势原理

1. **红黑树存储**：内核使用红黑树存储所有注册的fd，查找效率O(log n)
2. **就绪列表**：内核维护就绪fd的双向链表，epoll_wait直接返回就绪列表
3. **回调机制**：内核在fd就绪时通过回调函数将其加入就绪列表

**优势**：

- 事件驱动，只返回就绪的描述符
- 支持边缘触发（ET）和水平触发（LT）模式
- 高性能，适合大量并发连接

epoll是现代Linux高性能网络编程的核心技术，理解其工作原理和正确使用各种模式，是构建高性能服务器的关键。

```c
// 1. 创建epoll实例
int epfd = epoll_create1(0);

// 2. 添加监控的描述符
struct epoll_event ev;
ev.events = EPOLLIN;
ev.data.fd = sockfd;
epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

// 3. 等待事件
struct epoll_event events[MAX_EVENTS];
int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
```
## LT和ET模式

```c
```

