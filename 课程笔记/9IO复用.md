# I/O复用（I/O Multiplexing）

**I/O复用**是一种使单个进程/线程能够**同时监控多个I/O文件描述符**（如socket、文件、管道等）的机制，当其中任意一个描述符就绪（可读、可写或出现异常）时，程序就能及时处理，从而实现在单线程中处理多个I/O操作。

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
- `FD_ZERO(fd_set *fdse);`//清除fdset的所有位
- `FD_SET(int fd,fds_set *fdset);`//设置fdset的位fd
- `FD_CLR(int fd,fd_set *fdset);`//清除fdset的位fd(从集合中移除一个描述符fd)

- `int FD_ISSET(int fd,fd_set *fdset);`//测试fdset的位fd是否被设置（在select返回后，判断某个描述符fd是否依然处于集合中，即事件已发生）

- ## 参数

第一个参数`nfds`：是一个整数值，表示被监视的文件描述符的最大值加1。例如，如果我们要监视的文件描述符有3、5、8，那么最大是8，所以nfds应为9。这样内核就知道需要检查描述符0到8的集合，即使0、1、2、4、6、7 等 6 个无关的描述符没有被监视。

----缺点：每次调用需要复制整个描述符集合

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

## 使用select

```c
fd_set readfds;
int maxfd = listenfd;

while (1) {
    FD_ZERO(&readfds);
    FD_SET(listenfd, &readfds);      // 监听新连接
    for (int i = 0; i < client_count; i++) {
        FD_SET(client_fd[i], &readfds); // 监听已有客户端
        if (client_fd[i] > maxfd) maxfd = client_fd[i];
    }

    int ret = select(maxfd + 1, &readfds, NULL, NULL, NULL);
    if (ret < 0) {
        // 错误处理
    }

    // 检查新连接
    if (FD_ISSET(listenfd, &readfds)) {
        int newfd = accept(listenfd, ...);
        // 添加到 client_fd 数组
    }

    // 检查每个客户端是否有数据
    for (int i = 0; i < client_count; i++) {
        if (FD_ISSET(client_fd[i], &readfds)) {
            int n = recv(client_fd[i], buf, sizeof(buf), 0);
            if (n <= 0) {
                // 关闭连接，从数组中移除
            } else {
                // 处理数据（例如广播给其他客户端）
            }
        }
    }
}
```

1. **同时监视两种事件**：将监听套接字（新连接）和已连接套接字（客户端数据）都加入 `select` 的读集合，`select` 可以同时等待这两类事件的发生。
2. **第一个参数**：传入所有被监视描述符中**最大值 + 1**，这是 `select` 的要求，用于限定内核检查的范围。
3. **事件处理逻辑**：
   - 如果**监听套接字**可读，说明有**新连接**，调用 `accept` 取出并加入客户端数组。
   - 如果**某个已连接套接字**可读，说明有**客户端发来数据**，调用 `recv` 处理。
4. **对比不使用 `select`**：如果不使用多路复用，直接调用 `accept` 会**阻塞**直到有新连接，期间无法处理已连接客户端的读写，CPU 虽然不忙（进程休眠），但服务器失去了响应其他事件的能力。
5. **使用 `select` 的优势**：如果监听套接字没有触发可读事件（即使没有新连接），`select` 也可能因为已连接套接字可读 而返回，从而执行后续代码（处理客户端数据），避免了长期阻塞在 `accept` 上。

- `select` **本身也会阻塞**：`select` 调用会阻塞直到**至少一个被监视的描述符就绪**。因此程序仍然会“卡”在 select 上，但这次是同时等待多种事件，一旦返回就能区分处理。
- **CPU 利用率**：阻塞时 该进程让出 CPU，CPU不会空转，这与直接阻塞在 `accept` 类似。
  - 区别在于：阻塞在 `select` 上可以同时等待多个来源
  - （在`select`上更不容易阻塞，只要有新连接或者有已经建立的连接就不会阻塞）
    - 只要有一个被select监视的描述符就绪，就打破阻塞。
  - 而阻塞在 `accept` 上只能等待新连接。

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

- 创建epoll实例，**返回epoll文件描述符epfd**,将其作为其他所有epoll系统调用的第一个参数，用来指定要访问的内核事件表。
- `epoll_create1(0)`等同于`epoll_create(1)`

### 2. epoll_ctl

用于向指定的 epoll 实例（由 `epoll_create` 返回的文件描述符 `epfd`）**注册、修改或删除**对某个文件描述符 `fd` 的事件监控。简单来说，它告诉内核：“请开始监控这个 fd 上的某些事件”，或者“请停止监控这个 fd”，又或者“请修改这个 fd 的监控事件类型”。

通过 `epoll_ctl`，可以灵活地管理大量文件描述符，实现高效的 I/O 多路复用。

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

#### **events标志：**

- `EPOLLIN`：**可读事件**

#### **可读事件的具体触发条件包括：**

- **可读事件的具体触发条件包括：**

  - 接收缓冲区中有数据（字节数大于等于低水位标记，通常为1）；

  - 对端关闭了连接（此时读取会返回0，表示 EOF）；

  - 监听 socket 上有新的连接请求（accept 可读）；

  - 有错误发生但数据仍可读（如 TCP 带外数据）。

- **默认行为（水平触发，Level-Triggered）**：
  - 只要文件描述符处于可读状态，`epoll_wait` 就会**持续返回**该事件。这意味着如果你没有把数据读完（或者没有处理完可读条件），下一次调用 `epoll_wait` 仍然会立刻通知你。这种模式简单、不易遗漏事件，但可能会导致频繁唤醒（如果数据一直没读完）。

- `EPOLLOUT`：**可写事件**
- `EPOLLPRI`：紧急数据可读
- `EPOLLERR`：错误事件
- `EPOLLHUP`：挂起事件



- `EPOLLET`：**边缘触发模式**

  - `EPOLLET` 本身不是一个独立的事件类型，而是一个**模式标志**，通常与 `EPOLLIN`、`EPOLLOUT` 等组合使用（例如 `events = EPOLLIN | EPOLLET`）。它改变了 epoll 的事件通知方式：从水平触发变为**边缘触发**（Edge-Triggered）。

  - **边缘触发的核心规则**：
    - 只有当被监控的文件描述符的**状态发生变化**（即从“不可读”变为“可读”，或从“不可写”变为“可写”）时，epoll 才会通知应用程序一次。之后即使该描述符依然可读，只要没有新的数据到达（状态没有再次变化），epoll 就不会再产生通知。

- `EPOLLONESHOT`：**一次性事件**

- `EPOLLRDHUP`：对端关闭连接或半关闭

#### 注意事项

- 同一个 `fd` 不能重复添加（除非先删除再添加），否则会返回 `EEXIST` 错误。
- 修改事件时，`fd` 必须已经注册过。
- `epoll_ctl` 是同步操作，成功返回 0，失败返回 -1 并设置 `errno`。

### 3. epoll_wait

epoll系列系统调用的主要接口。在一段超时时间内等待一组文件描述符上的事件。

```c++
int epoll_wait(int epfd, struct epoll_event *events,
               int maxevents, int timeout);
```

- **等待事件发生**
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
- 支持**边缘触发**（ET）和**水平触发**（LT）模式
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

**默认行为（水平触发，Level-Triggered）**：

- 只要文件描述符处于可读状态，`epoll_wait` 就会**持续返回**该事件。这意味着如果你没有把数据读完（或者没有处理完可读条件），下一次调用 `epoll_wait` 仍然会立刻通知你。这种模式简单、不易遗漏事件，但可能会导致频繁唤醒（如果数据一直没读完）。

**非默认行为**（**边缘触发，Edge-Triggered**）：

- `EPOLLET` 本身不是一个独立的事件类型，而是一个**模式标志**，通常与 `EPOLLIN`、`EPOLLOUT` 等组合使用（例如 `events = EPOLLIN | EPOLLET`）。它改变了 epoll 的事件通知方式：从水平触发变为**边缘触发**（Edge-Triggered）。
- **边缘触发的核心规则**：
  - 只有当被监控的文件描述符的**状态发生变化**（即从“不可读”变为“可读”，或从“不可写”变为“可写”）时，epoll 才会通知应用程序一次。之后即使该描述符依然可读，只要没有新的数据到达（状态没有再次变化），epoll 就不会再产生通知。

**形象理解**：

- 水平触发（LT）：门一直开着，只要屋里有人（数据），你就不断被提醒。
- 边缘触发（ET）：门只在打开的那一瞬间提醒你一次，之后就算屋里一直有人，也不会再提醒，直到门再次关闭又打开（新数据到达）。

### EPOLLIN | EPOLLET 的结合使用

将 `EPOLLIN` 和 `EPOLLET` 组合，表示**以边缘触发的方式监听可读事件**。这种模式在高性能网络服务器中很常见，因为它可以减少 epoll 事件通知的次数，提升并发处理能力。但使用边缘触发时，开发者需要遵循一些重要准则：

- **必须非阻塞**：通常配合非阻塞 I/O 使用，因为一旦事件触发，你需要**循环读取**直到返回 `EAGAIN`（表示当前无数据可读），否则可能遗漏剩余数据。
- **一次性读完**：在收到可读事件后，要在一个循环内尽可能多地读取数据（例如反复调用 `read`/`recv`），直到返回 `EAGAIN`，确保内核缓冲区被清空，以便下一次新数据到达时能正确触发新事件。
- **小心处理饥饿**：由于只通知一次，如果读取不彻底，剩余数据将一直留在缓冲区，但 epoll 不会再通知，导致数据“饿死”。

# 常见的`errno`错误码

| 错误码名称                   | 数值 | 含义                                                         | 常见触发场景                                                 |
| :--------------------------- | :--- | :----------------------------------------------------------- | :----------------------------------------------------------- |
| **EPERM**                    | 1    | 操作不允许（Operation not permitted）                        | 需要超级用户权限的操作，但当前用户权限不足。                 |
| **ENOENT**                   | 2    | 没有那个文件或目录（No such file or directory）              | 打开不存在的文件、访问不存在的路径。                         |
| **EINTR**                    | 4    | 系统调用被信号中断（Interrupted system call）                | 阻塞的系统调用（如 `read`、`write`、`accept`）在执行期间收到了信号。 |
| **EIO**                      | 5    | I/O 错误（I/O error）                                        | 底层硬件或文件系统错误，如磁盘坏道。                         |
| **EBADF**                    | 9    | 坏的文件描述符（Bad file descriptor）                        | 使用未打开或已关闭的文件描述符进行操作。                     |
| **EAGAIN** / **EWOULDBLOCK** | 11   | 资源暂时不可用，操作会阻塞（Resource temporarily unavailable） | 非阻塞模式下，读无数据、写缓冲区满、`accept` 无新连接、`connect` 未完成等。 |
| **ENOMEM**                   | 12   | 无法分配内存（Cannot allocate memory）                       | 系统内存不足，或进程虚拟内存限制达到上限。                   |
| **EACCES**                   | 13   | 权限不足（Permission denied）                                | 文件或目录没有读/写/执行权限，或缺少搜索权限。               |
| **EFAULT**                   | 14   | 错误的地址（Bad address）                                    | 指针参数指向了非法内存区域（如未映射的地址）。               |
| **EBUSY**                    | 16   | 设备或资源忙（Device or resource busy）                      | 尝试挂载已挂载的设备、删除正在使用的文件等。                 |
| **EEXIST**                   | 17   | 文件已存在（File exists）                                    | 使用 `O_CREAT` 和 `O_EXCL` 创建文件时文件已存在。            |
| **ENODEV**                   | 19   | 没有此设备（No such device）                                 | 指定的设备文件不存在或对应的驱动未加载。                     |
| **ENOTDIR**                  | 20   | 不是目录（Not a directory）                                  | 在非目录的文件上执行了目录操作（如 `chdir`）。               |
| **EISDIR**                   | 21   | 是目录（Is a directory）                                     | 在目录上执行了文件写操作（如 `write`），但目录不支持写入。   |
| **EINVAL**                   | 22   | 无效参数（Invalid argument）                                 | 传给系统调用的参数非法，如错误的 `flags`、不合法的值等。     |
| **ENFILE**                   | 23   | 系统文件表已满（File table overflow）                        | 整个系统打开的文件描述符总数达到上限。                       |
| **EMFILE**                   | 24   | 进程文件描述符表已满（Too many open files）                  | 当前进程打开的文件描述符数量已达到 `RLIMIT_NOFILE` 限制。    |
| **ENOSPC**                   | 28   | 设备上无空间（No space left on device）                      | 磁盘已满，无法写入更多数据。                                 |
| **EPIPE**                    | 32   | 管道损坏（Broken pipe）                                      | 向已关闭读端的管道或 socket 写入数据，会导致进程收到 `SIGPIPE` 信号。 |
| **ERANGE**                   | 34   | 结果太大（Numerical result out of range）                    | 数学函数结果超出可表示范围，或字符串转换超出类型范围。       |
| **ENOSYS**                   | 38   | 功能未实现（Function not implemented）                       | 调用了内核未编译支持的系统调用。                             |
| **ENOTSOCK**                 | 88   | 不是套接字（Socket operation on non-socket）                 | 在非 socket 文件描述符上执行 socket 操作（如 `bind`、`listen`）。 |
| **EDESTADDRREQ**             | 89   | 需要目标地址（Destination address required）                 | 在未连接的 socket 上发送数据，但没有指定目标地址（如未调用 `connect`）。 |
| **EMSGSIZE**                 | 90   | 消息过长（Message too long）                                 | 发送的数据报超过 socket 的最大传输单元（MTU）。              |
| **EPROTOTYPE**               | 91   | 协议类型错误（Protocol wrong type for socket）               | socket 类型与协议不匹配（如为 SOCK_STREAM 指定了 UDP 协议）。 |
| **ENOPROTOOPT**              | 92   | 协议不可用（Protocol not available）                         | 设置了不支持的 socket 选项（如 `setsockopt` 使用无效选项）。 |
| **EPROTONOSUPPORT**          | 93   | 协议不支持（Protocol not supported）                         | 请求的协议（如指定 IPPROTO_SCTP）在内核中未实现。            |
| **EOPNOTSUPP**               | 95   | 操作不支持（Operation not supported）                        | 在 socket 上执行不支持的操作（如在 TCP 上调用多播相关选项）。 |
| **EAFNOSUPPORT**             | 97   | 地址族不支持（Address family not supported by protocol）     | 使用了协议不支持的地址族（如在 IPv4 socket 上传递 IPv6 地址）。 |
| **EADDRINUSE**               | 98   | 地址已使用（Address already in use）                         | `bind` 时端口或地址已被占用，且未设置 `SO_REUSEADDR`。       |
| **EADDRNOTAVAIL**            | 99   | 地址不可用（Cannot assign requested address）                | 绑定的 IP 地址不属于本地主机（例如绑定到非本地 IP）。        |
| **ENETDOWN**                 | 100  | 网络未启动（Network is down）                                | 网络接口已关闭或不可用。                                     |
| **ENETUNREACH**              | 101  | 网络不可达（Network is unreachable）                         | 路由表中没有到目标网络的路径。                               |
| **ECONNABORTED**             | 103  | 连接中止（Software caused connection abort）                 | 对端异常关闭连接（如超时、重传失败）。                       |
| **ECONNRESET**               | 104  | 连接重置（Connection reset by peer）                         | 对端突然关闭连接（如进程崩溃、发送 RST 包）。                |
| **ENOBUFS**                  | 105  | 无缓冲区空间（No buffer space available）                    | 系统内存不足，无法分配网络缓冲区。                           |
| **EISCONN**                  | 106  | 套接字已连接（Transport endpoint is already connected）      | 对已连接的 socket 再次调用 `connect`。                       |
| **ENOTCONN**                 | 107  | 套接字未连接（Transport endpoint is not connected）          | 在未建立连接的 socket 上执行读写操作（通常 TCP 会触发）。    |
| **ETIMEDOUT**                | 110  | 连接超时（Connection timed out）                             | 连接建立或数据传输超时（如长时间未收到对端确认）。           |
| **ECONNREFUSED**             | 111  | 连接拒绝（Connection refused）                               | 目标端口上没有监听服务，或目标主动拒绝连接（发送 RST）。     |
| **EHOSTUNREACH**             | 113  | 主机不可达（No route to host）                               | 路由表中没有到目标主机的路由。                               |
| **EINPROGRESS**              | 115  | 操作正在进行中（Operation now in progress）                  | 非阻塞 `connect` 正在建立连接，尚未完成。                    |
| **EDQUOT**                   | 122  | 磁盘配额超限（Disk quota exceeded）                          | 写入文件时超出用户或组的磁盘配额限制。                       |

- 当 `accept` 返回 `EAGAIN` 或 `EWOULDBLOCK` 时，表示已完成连接队列已空，应结束循环等待下一次事件。
- 当 `recv` 返回 `ECONNRESET` 时，说明对端已关闭连接，应关闭相应 socket。
- 当 `send` 返回 `EPIPE` 时，通常应捕获 `SIGPIPE` 信号或忽略该错误，避免进程意外退出。
