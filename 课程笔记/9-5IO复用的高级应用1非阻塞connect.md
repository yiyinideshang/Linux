# I/O复用的高级应用一：非阻塞connect

# 代码示例

> 代码清单 9-5 非阻塞connect

````c
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#define BUFFER_SIZE 1023

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

int unblock_connect(const char* ip,int port,int time)
{
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET,ip,&address.sin_addr);
    address.sin_port = htons(port);

    int sockfd = socket(PF_INET,SOCK_STREAM,0);
    int fdopt = setnonblocking(sockfd);
    ret = connect(sockfd,(struct sockaddr*)&address,sizeof(address));
    if(ret == 0)
    {
        printf("连接成功，立即返回\n");
        fcntl(sockfd,F_SETFL,fdopt);
        return sockfd;
    }
    //else:ret == -1时：区分两种情况
    //非阻塞I/O的系统调用(如connect)总是立即返回，不管事件是否已经返回，如果事件没有立即发生，则系统调用返回-1，与出错情况一样
    //此时必须通过errno来区分这两种情况，
    //对于connect而言，事件还未发生时   系统调用返回-1errno被设置为EINPROGRESS(意为“在处理中”);
    //否则默认表示 出错，系统调用返回-1,
    //下面代码则表示如果errno不是被设置为 “在处理中”，则表示出错
    else if(errno != EINPROGRESS)
    {
        printf("connect连接出错\n");
        close(sockfd);
        return -1;
    }
    else
    {
        printf("连接正在进行....\n");
    }
    fd_set writefds;
    fd_set readfds;
    struct timeval timeout;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(sockfd,&writefds);

    timeout.tv_sec = time;
    timeout.tv_usec = 0;

    ret = select(sockfd+1,NULL,&writefds,NULL,&timeout);
    if(ret <= 0)
    {
        printf("select超时或错误\n");
        close(sockfd);
        return -1;
    }

    if(!FD_ISSET(sockfd,&writefds))
    {
        printf("sockfd不在集合中，不可写\n");
        close(sockfd);
        return -1;
    }
    
    int error = 0;
    socklen_t length = sizeof(error);
    if(getsockopt(sockfd,SOL_SOCKET,SO_ERROR,&error,&length))
    {
        printf("getsockopt失败\n");
        close(sockfd);
        return -1;
    }
    if(error!=0)
    {
        printf("套接字有错，连接出错%d\n",error);
        close(sockfd);
        return -1;
    }
    printf("在使用套接字进行选择后，连接已准备就绪：%d\n",sockfd);
    fcntl(sockfd,F_SETFL,fdopt);
    return sockfd;
}
int main(int argc,char* argv[])
{
    if(argc<=2)
    {
        printf("在路径:%s 之后:需要加上IP地址和端口号\n",basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int sockfd = unblock_connect(ip,port,10);
    if(sockfd<0)
    {
        return 1;
    }
    close(sockfd);
    return 0;
}
````

# 编译运行

- # 1

```bash
gcc -o 9-5非阻塞connect 9-5非阻塞connect.c 
./9-5非阻塞connect 
```

**终端返回**：

```bash
在路径:9-5非阻塞connect 之后:需要加上IP地址和端口号
```

- ## 2

```bash
gcc -o 9-5非阻塞connect 9-5非阻塞connect.c
./9-5非阻塞connect 127.0.0.1 9999
```

**终端返回**：

```
连接正在进行....
套接字有错，连接出错111
```

- ## 3 使用 starce字段

```bash
strace -e connect ./9-5非阻塞connect 127.0.0.1 9999
```

## `starce`工具

`strace` 是一个强大的系统调用跟踪工具，它可以捕获并打印进程执行期间调用的所有系统调用（如 `read`、`write`、`connect` 等）及其返回值。常用于调试、性能分析和理解程序与内核的交互。

- **`-e connect`** 是跟踪过滤器，表示 **只跟踪 `connect` 这个系统调用**，其他系统调用一概不输出。这能让你专注观察 `connect` 的执行细节，避免被海量其他调用干扰。
- **`./9-5非阻塞connect`** 是要运行的程序，它接受两个参数 `127.0.0.1` 和 `9999`。

当你运行这个命令时，`strace` 会启动目标程序，并立即显示每次 `connect` 系统调用的参数、返回值和耗时。对于非阻塞 `connect` 的典型场景，你会看到第一次 `connect` 立即返回 `-1`（错误码 `EINPROGRESS`，对应值 115），表示连接正在后台建立。稍后程序可能会通过 `select`/`poll` 检测套接字可写，再用 `getsockopt` 检查最终连接结果——但这里仅跟踪 `connect`，所以后续系统调用不会被打印。

简单说，这条命令让你**专门观察非阻塞 `connect` 的发起过程**，非常适合验证非阻塞 `connect` 是否真的没有阻塞、返回了预期的错误。

**终端返回**：

```bash
connect(3, {sa_family=AF_INET, sin_port=htons(9999), sin_addr=inet_addr("127.0.0.1")}, 16) = -1 EINPROGRESS (操作现在正在进行)
连接正在进行....
套接字有错，连接出错111
+++ exited with 1 +++
```

- ## 4监听对应端口

`strace` 输出显示 `connect` 返回 `-1 EINPROGRESS`，这是**非阻塞 `connect` 完全正常的行为**。
之后程序打印 `“套接字有错，连接出错111”`，是因为 `getsockopt(SO_ERROR)` 返回 `error = 111`（即 `ECONNREFUSED`），表示**连接被对端拒绝**——**这正是您测试端口 9999 且无服务监听时应有的正确行为**，**程序没有任何错误**，它正确地检测到了连接失败并给出了明确原因。

这个提示恰恰说明您的程序**工作正常**。
如果您**期望看到连接成功的输出**，只需**启动一个真正的服务器监听对应端口**即可。

**在另一个终端启动测试服务器**（例如监听 9999 端口）：

```bash
nc -l 127.0.0.1 9999
```

**注意**：（保持该终端运行，不要按 Ctrl+C）

再次运行程序

```bash
yishang@yishang-virtual-machine:~/文档/Linux高性能服务器编程$ strace -e connect ./9-5非阻塞connect 127.0.0.1 9999
```

**终端返回**：

```bash
connect(3, {sa_family=AF_INET, sin_port=htons(9999), sin_addr=inet_addr("127.0.0.1")}, 16) = -1 EINPROGRESS (操作现在正在进行)
连接正在进行....
在使用套接字进行选择后，连接已准备就绪：3
+++ exited with 0 +++
```

- **运行客户端**：`./9-5非阻塞connect 127.0.0.1 9999`
  输出：`在使用套接字进行选择后，连接已准备就绪：3` ✅ 成功
- **不启动服务器**：`./9-5非阻塞connect 127.0.0.1 9999`
  输出：`套接字有错，连接出错111` ✅ 正确检测拒绝连接

## 运行结果截图

![非阻塞connect运行结果](D:\Typora\typora_work\Linux高性能服务器编程\非阻塞connect运行结果.png)

# 代码详解

## 一、整体功能

- **函数名**：`unblock_connect(const char* ip, int port, int time)`
- **参数**：服务器 IP、端口、超时秒数
- **返回值**：成功 → 已连接（阻塞模式）的 socket 描述符；失败 → -1
- **核心思想**：
  1. 将 socket 设为非阻塞。
  2. 调用 `connect` —— 通常立即返回 `-1`，`errno = EINPROGRESS`。
  3. 用 `select` 在指定超时时间内等待 socket 变为**可写**（表示连接过程完成）。
  4. 通过 `getsockopt(SO_ERROR)` 获取连接结果（成功或失败）。
  5. 恢复 socket 原始阻塞状态，返回描述符。

## 二、函数解析

### 1. `int setnonblocking(int fd)`

```c
int old_option = fcntl(fd, F_GETFL);   // 获取当前文件状态标志
int new_option = old_option | O_NONBLOCK; // 添加非阻塞标志
fcntl(fd, F_SETFL, new_option);        // 设置新标志
return old_option;                     // 返回原始标志，便于后续恢复
```

**作用**：将文件描述符设为非阻塞，并返回修改前的状态标志。
**典型用途**：在非阻塞 `connect` 前调用，在连接成功后用 `fcntl(fd, F_SETFL, old_option)` **恢复阻塞模式。**

### 2. `int unblock_connect(const char* ip, int port, int time)`

**核心函数**，实现非阻塞连接的全流程。

#### ① 创建 socket，设为非阻塞

```c
int sockfd = socket(PF_INET, SOCK_STREAM, 0);
int fdopt = setnonblocking(sockfd);
```

**可能错误**：`socket()` 失败（如达到文件描述符上限）→ 返回 -1（代码中未显式检查，建议补充）。

#### ② 发起非阻塞 `connect`

```c
ret = connect(sockfd, (struct sockaddr*)&address, sizeof(address));
```

- **立即成功（极少见）**：`ret == 0`
  - 通常发生在**本地回环且服务器负载极低**时，三次握手瞬时完成。
  - **操作**：恢复阻塞标志，直接返回 `sockfd`。✅ **中途返回成功路径**。
- **立即失败（非 EINPROGRESS）**：`ret == -1 && errno != EINPROGRESS`
  - 例如：无效参数、地址族错误等。
  - **操作**：关闭 socket，返回 -1。❌ **中途返回失败路径**。
- **正常情况**：`ret == -1 && errno == EINPROGRESS`
  - 表示连接正在后台进行，需要后续用 `select` 等待完成。
  - **继续向下执行**。

#### ③ 准备 `select` 监控

```c
fd_set writefds;
FD_ZERO(&writefds);      
FD_SET(sockfd, &writefds);

struct timeval timeout;
timeout.tv_sec = time;
timeout.tv_usec = 0;
```

- **必须用 `FD_ZERO` 清空集合**，否则 `writefds` 包含栈上的随机数据，可能导致 `select` 行为不可预测。
- **超时结构体**：`time` 单位是**秒**，由参数传入。

#### ④ `select` 等待可写事件

```c
ret = select(sockfd+1, NULL, &writefds, NULL, &timeout);
```

- **监控条件**：socket 变为**可写**（连接已完成，无论成功/失败）。
- **返回值处理**：
  - `ret == -1`：`select` 出错（如 EBADF、ENOMEM）。❌ **返回失败**。
  - `ret == 0`：超时时间内 socket 一直不可写。❌ **返回失败**。
  - `ret > 0`：至少一个描述符就绪，需要进一步检查。

 **您的代码未处理 `EINTR`**：若 `select` 被信号中断会直接返回 -1 并失败。健壮做法是用 `do-while` 重试。

#### ⑤ 防御性检查：`FD_ISSET`

```c
if(!FD_ISSET(sockfd, &writefds)) { ... }
```

- **即使 `ret > 0`，就绪的也不一定是 `sockfd`**（虽然目前集合中只有它）。

`FD_ISSET`函数返回一个 整数（int）

- 返回**非零值（通常为 1）**表示指定的文件描述符在集合中，**就绪/可用**。

- 返回**值零 **表示指定的文件描述符不在集合中，**未就绪/不可用**。若未就绪 ，说明内核状态异常或编程错误，不应继续。❌ **返回失败** 。

#### ⑥ 获取异步连接结果：`getsockopt(SO_ERROR)`

```c
int error = 0;
socklen_t length = sizeof(error);
if(getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &length)) { ... }
```

- **`getsockopt` 返回 0 表示成功获取，非 0 表示函数本身失败**。
- **将套接字上挂起的错误码复制到 `error` 变量中，同时清除内核中的错误状态**。
- **检查 `error`**：
  - `error == 0`：连接成功！
  - `error != 0`：连接失败，`error` 是具体错误码（如 `ECONNREFUSED = 111`）。❌ **返回失败**。

#### ⑦ 连接成功：恢复阻塞标志，返回 socket

```c
fcntl(sockfd, F_SETFL, fdopt);
return sockfd;
```

- 恢复 socket 原始阻塞/非阻塞状态。
- 主调函数获得一个**已连接且阻塞模式**的 socket，可直接用于 `read`/`write`。✅ **成功返回路径**。

### 3. `int main(int argc, char* argv[])`

- **参数检查**：要求至少提供 IP 和端口。
- **调用 `unblock_connect`**，超时 10 秒。
- 无论成功或失败，最后都 `close(sockfd)`（仅用于测试，实际应用中不会立即关闭）。
- 退出码：0 成功，1 失败。

## 各种return

| 条件                                              | 返回值           | 说明                                                |
| :------------------------------------------------ | :--------------- | :-------------------------------------------------- |
| `connect` 立即成功                                | `sockfd`         | 极少数情况，直接返回                                |
| `connect` 立即失败且 `errno != EINPROGRESS`       | `-1`             | 参数错误等                                          |
| ==`connect`没有立即发生，`errno == EINPROGRESS`== | `-1`             | 表示：“正在处理中”                                  |
| `select` 返回 -1                                  | `-1`             | 系统调用失败                                        |
| `select` 返回 0                                   | `-1`             | 超时                                                |
| ==`select`返回 >0==                               | 描述符就绪个数   | 至少一个描述符就绪，需要进一步检查。                |
| `FD_ISSET(sockfd)` 不成立                         | `0`              | 表示指定的文件描述符不在集合中，**未就绪/不可用**。 |
| `FD_ISSET(sockfd)` 成立                           | 非`0`值，通常为1 | 表示指定的文件描述符在集合中，**就绪/可用**。       |
| `getsockopt!=0` 函数本身失败                      | `-1`             | 选项层无效等                                        |
| `getsockopt==0` 返回的 `error != 0`               | `0`              | 表示`getsockopt`成功获取，连接失败（如拒绝、超时）  |
| ==`getsockopt==0` 返回的 `error == 0`==           | `0`              | 表示`getsockopt`成功获取,连接成功                   |
| **所有检查通过**                                  | `sockfd`         | **连接成功**                                        |

## 非阻塞 `connect` 核心原理回顾

- **为什么 `connect` 会返回 `EINPROGRESS`？**
  TCP 连接需要三次握手，耗时不可控。非阻塞模式下，`connect` 不会等待握手完成，而是立即返回 `EINPROGRESS`，连接过程由内核在后台继续。

- **为什么用 `select` 监控“可写”事件？**
  内核约定：**当连接过程结束时（无论成功/失败），套接字会变为可写**（部分系统同时变为可读）。这是内核给应用程序的统一“完成信号”。
- **为什么必须再用 `getsockopt(SO_ERROR)`？**
  可写事件**仅表示“连接已完成”，不表示“连接成功”**。必须通过 `SO_ERROR` 获取异步错误码才能区分成功与失败。
- **为什么恢复原始文件状态标志？**
  非阻塞模式仅用于连接过程，连接成功后通常希望 socket 工作在默认阻塞模式，以简化后续 I/O 逻辑。