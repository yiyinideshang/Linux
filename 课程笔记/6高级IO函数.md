- 用于创建文件描述符的函数，包括pipe、dup、dup2函数
- 用于读写数据的函数，包括readv/writev、sendfile、mmap/munmap、splice和tee函数 
- 用于控制I/O行为和属性的函数，包括fcntl函数

# 6.1pipe函数

==100页==

**创建一个管道，实现进程间通信**

```c
#include <unistdh>
int pipe(int fd[2]);
```

socket基础API中 有一个 socketpair函数。用来方便地创建双向管道

```c
#include <sys/types.h>
#include <sys/socket.h>
int socketpair(int domain,int type,int protocol,int fd[2]);
```

- 前三个参数与socket系统调用的三个参数完全相同，但是domain只能使用unix本地协议族**AF_UNIX**，因为我们仅能在**本地使用**这个双向管道
- 最后一个参数与pipe系统调用的参数一样，只不过socketpair创建的这对文件描述符是**即可读也可写**的。
- socketpair成功返回0，失败返回-1,并设置errno。

==第十三章 13.4节 讨论 如何使用管道来实现进程间通信==

# 6.2dup函数和dup2函数

==101页==

**将标准输入重定向到一个文件，或者把标准输出重定向到一个网络连接**

```c
#include <unistd.h>
int dup(int file_descriptor);
int dup2(int file_descriptor_one,int file_descriptor_two);
```

- dup函数创建一个新的文件描述符，该新的文件描述符与原有文件描述符file_descriptor指向相同的文件、管道或网络连接。并且dup返回的文件描述符总是取系统当前可用的最小整数值

- dup2和dup类似，不过它返回第一个不小于 file_descriptor_two的整数值
- dup和dup2系统调用失败返回-1并设置errno

**注意**：

- 通过dup和dup2创建的文件描述符 并不继承 原文件描述符的属性，比如 close-on-exec和non-blocking等

CGI：（common gateway interface ）通用网关接口，是一种标准协议

## 利用dup函数实现 CGI服务器

```

```

早期动态网页技术的基础

==102页==

标准输出文件符：STDOUT_FILENO（值为1）

# 6.3readv函数和writev函数

==103页==

**分散读，集中写**

```c
#include <sys/uio.h>
ssize_t readv(int fd,const struct iovec* vector, int count);
ssize_t writev(int fd, const struct iovec* vector,int count);
```

- fd参数：被操作的目标文件描述符
- 结构体iovec -==86页==，该结构体描述一段内存区
- count参数：vector数组的长度，即有多少块内存数据需要从fd读出或写到fd。

readv和wirtev在成功时返回读出/写入的fd字节数，失败返回-1，并设置errno

相当于简化的 recvmsg和sendmsg函数



- 将数据从文件描述符读取到分散的内存中

- 将多块分散的内存数据一并写入文件描述符中

## Web服务器上的集中写示例

```

```

==104页==

`stat`结构体

```c
int fstat(int fd,struct stat* buf)
//buf：指向struct stat结构体的指针
```

==105页==

```c
ssize_t read(int fd,void* buf,size_t count)
ssize_t wirte(int fd,void* buf,size_t count)
```

- buf: ①读取数据存放的缓冲区指针②要写入的数据缓冲区指针
- count：①请求读取的字节数②请求写入的字节数

# 6.4sendfile函数

==106页==

**在两个文件描述符之间直接传递数据**（完全在内核中操作），避免了内核缓冲区和用户缓冲区之间的数据拷贝，效率很高，被称为 **零拷贝**

通过减少数据拷贝次数和上下文切换，显著提升了文件传输性能。见后文**sendfile 零拷贝**

```c
#include <sys/sendfile.h>
ssize_t sendfile(int out_fd,int in_fd,off_t* offset,size_t count);
```

- in_fd参数：是待读出内容的文件描述符
- out_fd参数：待写入内容的文件描述符
- offset参数：指定从读入文件流的哪个位置开始读，如果为空，则使用读入文件流默认的起始位置
- count参数：指定在文件描述符in_fd和out_fd之间传输的字节数。

sedfile成功时返回传输的字节数，失败返回-1并设置errno

**注意**：

- in_fd必须是一个支持类似于mmap函数的文件描述符，即它必须指向真正的文件，不能是socket和管道
- out_fid则必须是一个socket
- sendfile几乎是专门为在网络上传输文件而设计的

## 用sendfile函数传递文件

```

```

# 6.5mmap函数和munmap函数

==107页==

mmap函数用于申请一段内存空间。可以**将这段内存作为进程间通信的共享内存，也可以将文件直接映射到其中。**

munmap函数则**释放由mmap创建的这段内存空间**

```c
#include <sys/mman.h>
void* mmap(void *start,size_t length,int prot,int flags,int fd, off_t offset);
int munmap(void* start, size_t length);
```

- start参数允许用户使用某个特定的地址作为这段内存的起始地址。如果被设置为null,则系统自动分配一个地址
- length参数指定这段内存段的长度
- prot参数用来设置内存段的访问权限，可以取下面几个值 的按位或：
  - PROT_READ,内存段可读
  - PROT_WRITE，内存段可写
  - PROT_EXEC,内存段可执行
  - PROT_NONE，内顿段不可访问
- flags参数控制内存段内容被修改后程序的行为。可以被设置为下标中的某些值的按位或(MAP_SHARED和MA_PRIVATE是互斥的，不能同时指定)

| 常用值          | 含义 |
| --------------- | ---- |
| `MAP_SHARED`    |      |
| `MAP_PRIVATE`   |      |
| `MAP_ANONYMOUS` |      |
| `MAP_FIXED`     |      |
| `MAP_HUGETLB`   |      |

- fd参数 是被映射文件对应的文件描述符。它一般通过open系统调用获得。
- offset参数设置从文件的何处开始映射

mmap函数成功时返回指向目标内存区域的指针，失败则返回`MAP_FAILED((void*)-1)`

并设置errno

munmap函数成功时返回0，失败时返回-1并设置errno

==第十三章 讨论 如何利用mmap函数来实现进程间共享内存==

# 6.6splice函数

==108页==

**用于两个文件描述符之间移动数据**，也是**零拷贝**操作

```c
#include <fcntl.h>
ssize_t splice(int fd_in,loff_t* off_in,int fd_out,loff_t* off_out,size_t len,unsigned int flags);
```

- flags参数的常用值及其含义

| 常用值            | 含义 |
| ----------------- | ---- |
| SPLICE_F_MOVE     |      |
| SPLICE_F_NONBLOCK |      |
| SPLICE_F_MORE     |      |
| SPLICE_F_GIFT     |      |

splice 函数可能产生的errno及其含义

| 错误   | 含义 |
| ------ | ---- |
| EBADF  |      |
| EINVAL |      |
| ENOMEM |      |
| ESPIPE |      |

## 使用splice函数实现回射服务器

```

```

# 6.7tee函数

==110页==

在两个管道文件描述符之间 复制数据，也是零拷贝操作

不消耗数据，因此源文件描述符上的数据仍然可以用于后续的读操作

```c
#include <fcntl.h>
ssize_t tee(int fd_in,int fd_out,size_t len,unsigned int flags);
```

该函数的参数的含义与splice相同（但fd_in和fd_out必须都是管道文件描述符）。

tee函数成功时返回两个文件描述符之间复制的数据数量（字节数），返回0表示没有复制任何数据。失败时返回-1,并设置errno

## 同时输出数据到终端和文件的程序

# 6.8 fcntl函数与ioctl函数

==112页==

**`fcntl()` 函数** 是 Unix/Linux 系统中用于**文件控制**（File Control）的系统调用，提供了对已打开文件描述符的各种控制操作。

另一个常见的控制文件描述符属性和行为的系统调用是**ioctl函数**，它比fcntl执行更多的控制。

但是，对于控制文件描述符常用的属性和行为，fcntl函数是由POSIX规范指定的首选方法

在网络编程中，fcntl函数通常用来将一个文件描述符设置为非阻塞的

## 函数原型

```c
#include <fcntl.h>
int fcntl(int fd,int cmd,.../* arg */);
```

- **fd参数**：被操作的文件描述符
- **cmd参数**：执行何种类型的操作，**常见操作见下表**
- **arg参数**：可选参数

fcntl函数支持的常用操作以及参数如下表

| 操作分类                           | 操作            | 含义                                                         | 第三个参数的类型 | 成功时的返回值                    |
| ---------------------------------- | --------------- | ------------------------------------------------------------ | ---------------- | --------------------------------- |
| 复制文件描述符                     | F_DUPFD         |                                                              | `long`           | 新创建的文件描述符的值            |
|                                    | F_DUPFD_CLOEXEC |                                                              | `long`           | 新创建的文件描述符的值            |
| 获取和设置文件描述符的**标志**     | F_GET**FD**     | 获取fd的标志，如`close-onn-exec`标志                         | 无               | fd的标志                          |
|                                    | F_SET**FD**     |                                                              | `long`           | 0                                 |
| 获取和设置文件描述符的**状态标志** | **F_GETFL**     | 获取fd的状态标志，这些标志包括可由open系统调用设置的标志（O_APPEND、OCARTE等）和 访问模式（O_RDONLY、O_WRONLY和O_RDWR） | void             | fd的状态标志                      |
|                                    | **F_SETFL**     | 设置fd的状态标志，但是部分标志是不能被修改的（如访问模式标志） | `long`           | 0                                 |
| 管理信号                           | F_GETOWN        |                                                              | 无               | 信号的宿主进程的PID或进程组的组ID |
|                                    | F_SETOWN        |                                                              | `long`           | 0                                 |
|                                    | F_GETSIG        |                                                              | 无               | 信号值，0表示SIGIO                |
|                                    | F_SETSIG        |                                                              | `long`           | 0                                 |
| 操作管道容量                       | F_SETPIPE_      |                                                              | `long`           | 0                                 |
|                                    | F_GETPIPE_SZ    |                                                              | 无               | 管道容量                          |

## 主要操作、主要功能

### ==1. 文件描述符标志 控制==

使用fcntl函数 设置 文件描述符标志(**File Descriptor Flags)**

这个标志**附着在文件描述符本身**，而不是打开的文件上。

==详见`D:\Typora\typora_work\5网络编程\1.1socket套接字\socket.md`==

- **FD_CLOEXEC**（唯一重要的fd标志） 设置已打开文件描述符的 close-on-exec 标志

```c
// 获取文件描述符标志
int flags = fcntl(fd, F_GETFD);
// 设置文件描述符标志 ---打开后设置
fcntl(fd, F_SETFD, flags | FD_CLOEXEC);// 设置close-on-exec
```

### ==2. **文件状态标志 控制**==

**File Status Flags**

- 这些标志**附着在打开的文件表项**上，影响I/O操作的行为。

```c
// 获取文件状态标志
int flags = fcntl(fd, F_GETFL);
// 添加非阻塞标志
fcntl(fd, F_SETFL, flags | O_NONBLOCK);//O_NONBLOCK：非阻塞I/O
// 添加同步写入标志
fcntl(fd, F_SETFL, flags | O_SYNC);//O_SYNC / O_DSYNC：同步写入
```

### 3. **文件锁定**

```c
struct flock lock;
lock.l_type = F_WRLCK;    // 写锁
lock.l_whence = SEEK_SET;
lock.l_start = 0;
lock.l_len = 100;         // 锁定前100字节

// 设置文件锁
fcntl(fd, F_SETLK, &lock);    // 非阻塞
fcntl(fd, F_SETLKW, &lock);   // 阻塞等待
```

### 4. **复制文件描述符**

```c
// 复制文件描述符
int new_fd = fcntl(old_fd, F_DUPFD, 0);
// 复制并指定最小文件描述符值
int new_fd = fcntl(old_fd, F_DUPFD, 10);  // 返回≥10的fd
```

## 常用场景

### ==设置非阻塞I/O==---2.文件状态标志

- 将fcntl第三个参数设置为：`flags | O_NONBLOCK`

```c
int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
```

### 实现文件锁 ----3.文件锁定

```c
int lock_file(int fd) {
    struct flock fl = {
        .l_type = F_WRLCK,
        .l_whence = SEEK_SET,
        .l_start = 0,
        .l_len = 0  // 锁定整个文件
    };
    return fcntl(fd, F_SETLK, &fl);
}
```

## 与 `ioctl()` 的区别

| 特性         | `fcntl()`          | `ioctl()`          |
| :----------- | :----------------- | :----------------- |
| **标准化**   | POSIX标准          | 非标准，设备相关   |
| **用途**     | 文件描述符通用控制 | 设备特定操作       |
| **参数**     | 相对固定           | 高度可变，依赖设备 |
| **可移植性** | 高                 | 低                 |

```c
#include <fcntl.h>
#include <unistd.h>

int fcntl(int fd, int cmd, ...  );
```

## 返回值

- 成功：根据cmd不同返回不同的值
- 失败：返回-1，并设置errno

`fcntl()` 是Unix/Linux系统编程中非常重要的函数，特别在网络编程、多进程编程和文件操作中广泛使用。

# 管理信号	SIGIO 和 SIGURG

==信号的使用在第10章讨论==

```c
void (*signal(int sig, void (*func)(int)))(int);
```

```c
typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);
```

它接受两个参数：

- `int sig`：要处理的信号编号。
- `sighandler_t handler`：一个**函数指针**，指向一个接受 `int` 参数、返回 `void` 的函数。

## **SIGIO** - 异步 I/O 就绪信号

**触发条件**：当文件描述符（通常是套接字或终端）可以进行非阻塞I/O时

## **SIGURG** - 紧急/带外数据信号

**触发条件**：当套接字接收到带外（Out-of-Band）数据时

## **宿主进程 (Owning Process)**

- **定义**

宿主进程是通过 `fcntl(fd, F_SETOWN, pid)` 设置的，将接收特定信号（SIGIO, SIGURG）的进程或进程组。

- 设置方式

```c
#include <unistd.h>

// 1. 设置为当前进程
fcntl(fd, F_SETOWN, getpid());

// 2. 设置为指定进程ID
fcntl(fd, F_SETOWN, 1234);

// 3. 设置为进程组（负值）
fcntl(fd, F_SETOWN, -getpgid(0));  // 当前进程组
fcntl(fd, F_SETOWN, -1234);        // 进程组ID 1234
```

- 查询宿主进程

```c
pid_t owner = fcntl(fd, F_GETOWN);
if (owner > 0) {
    printf("Owning process: %d\n", owner);
} else if (owner < 0) {
    printf("Owning process group: %d\n", -owner);
}
```

### 示例	信号驱动I/O

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>

int sockfd;

void sigio_handler(int sig) {
    printf("SIGIO: I/O ready\n");
    char buffer[1024];
    ssize_t n = read(sockfd, buffer, sizeof(buffer));
    if (n > 0) {
        printf("Read %zd bytes\n", n);
        // 处理数据...
    }
}

int main() {
    // 1. 创建套接字
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    // 2. 绑定和监听...
    struct sockaddr_in addr = {...};
    bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    listen(sockfd, 5);
    
    // 3. 设置信号处理
    struct sigaction sa;
    sa.sa_handler = sigio_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  // 系统调用自动重启
    sigaction(SIGIO, &sa, NULL);
    
    // 4. 设置宿主进程和异步I/O
    fcntl(sockfd, F_SETOWN, getpid());
    int flags = fcntl(sockfd, F_GETFL);
    fcntl(sockfd, F_SETFL, flags | O_ASYNC | O_NONBLOCK);
    
    printf("Waiting for connections and data...\n");
    
    // 5. 主循环等待信号
    while (1) {
        pause();  // 等待信号
    }
    
    return 0;
}
```

## 应用场景

1. **高性能网络服务器**：替代select/poll的轮询
2. **带外数据通知**：实时处理紧急数据
3. **终端输入**：异步读取用户输入
4. **设备驱动**：硬件事件通知

与 `select()`/`poll()`/`epoll()` 的比较

| 方法            | 机制         | 效率         | 复杂度         |
| :-------------- | :----------- | :----------- | :------------- |
| **信号驱动I/O** | 事件触发信号 | 高（无轮询） | 低（编程简单） |
| **select/poll** | 轮询检查     | 中           | 中             |
| **epoll**       | 事件通知     | 最高         | 高             |

**注意**：现代高性能服务器更多使用 `epoll`，但信号驱动I/O在某些特定场景（如终端、简单协议）仍有其价值。

# ==---------------------==

# 零拷贝函数（Zero-copy Functions）

**零拷贝** 是一种技术，旨在减少数据在内存中的拷贝次数，从而**降低CPU使用率、减少内存带宽占用、提升I/O性能**。

### 传统的数据传输流程（非零拷贝）
以从磁盘文件发送到网络为例：
```c
磁盘文件 → 内核缓冲区 → 用户缓冲区 → Socket缓冲区 → 网络
```
- ## **步骤1：磁盘文件 → 内核缓冲区**

  - **操作类型**：数据拷贝

  - **执行者**：DMA控制器（Direct Memory Access）

  - **说明**：这是**DMA拷贝**，不经过CPU，不占用CPU资源


- ## **步骤2：内核缓冲区 → 用户缓冲区**

  - **触发方式**：系统调用 `read()`

  - **包含两个动作**：
    1. ==**系统调用**==：`read(fd, user_buf, size)` - 用户进程主动发起
    2. **数据拷贝**：**CPU拷贝**，CPU将数据从内核缓冲区复制到用户缓冲区


- ## **步骤3：用户缓冲区 → Socket缓冲区**

  - **触发方式**：系统调用 `write()`

  - **包含两个动作**：
    1. ==**系统调用**==：`write(socket_fd, user_buf, size)` - 用户进程主动发起
    2. **数据拷贝**：**CPU拷贝**，CPU将数据从用户缓冲区复制到Socket缓冲区


- ## **步骤4：Socket缓冲区 → 网络**

  - **操作类型**：数据拷贝

  - **执行者**：DMA控制器

  - **说明**：这是**DMA拷贝**，网卡直接从Socket缓冲区读取数据发送


经历了：**2次系统调用（read + write）和 4次数据拷贝**（2次CPU拷贝 + 2次DMA拷贝），**4次上下文切换次数**：

1. 用户态 → 内核态（调用read）
2. 内核态 → 用户态（read返回）
3. 用户态 → 内核态（调用write）
4. 内核态 → 用户态（write返回）

| 步骤                         | 操作                | 类型               | 执行者    | CPU参与 |
| :--------------------------- | :------------------ | :----------------- | :-------- | :------ |
| 1. 磁盘 → 内核缓冲区         | 数据拷贝            | DMA拷贝            | DMA控制器 | 否      |
| 2. 内核缓冲区 → 用户缓冲区   | 系统调用 + 数据拷贝 | 系统调用 + CPU拷贝 | CPU       | 是      |
| 3. 用户缓冲区 → Socket缓冲区 | 系统调用 + 数据拷贝 | 系统调用 + CPU拷贝 | CPU       | 是      |
| 4. Socket缓冲区 → 网络       | 数据拷贝            | DMA拷贝            | DMA控制器 | 否      |

### 零拷贝的核心思想
避免数据在**内核空间和用户空间之间的冗余拷贝**，让数据直接在**内核空间内部流转**。

---

## sendfile 零拷贝

### sendfile 的工作流程（Linux 2.1+）
```c
#include <sys/sendfile.h>
ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count);
```
**数据传输路径**：
```
磁盘文件 → 内核缓冲区（PageCache） → Socket缓冲区 → 网络
```
- **减少到 3 次数据拷贝**（1次CPU拷贝 + 2次DMA拷贝）
- **只有 1 次系统调用**

### 演进：真正的零拷贝（Linux 2.4+）
借助 **DMA Gather Copy** 和 **Scatter-Gather DMA** 技术：
```
磁盘文件 → 内核缓冲区 → 网络（直接DMA传输）
```
- **减少到 2 次DMA拷贝**，完全消除CPU拷贝
- 内核只需要传递**文件描述符和元数据**给网卡

---

## 其他零拷贝技术

### 1. **mmap + write**
将文件映射到用户空间，避免一次拷贝：
```c
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
```
流程：
```
磁盘 → 内核缓冲区 → 用户空间映射区域 → Socket缓冲区 → 网络
```
减少一次拷贝，但仍需系统调用。

### 2. **splice**（Linux 2.6+）
在两个文件描述符之间移动数据，无需用户空间参与：
```c
ssize_t splice(int fd_in, off_t *off_in, int fd_out, off_t *off_out, size_t len, unsigned int flags);
```
可用于管道、文件、套接字间的数据传输。

### 3. **TCP_NODELAY + MSG_ZEROCOPY**（Linux 4.14+）
```c
setsockopt(fd, SOL_SOCKET, SO_ZEROCOPY, &1, sizeof(1));
send(fd, buf, len, MSG_ZEROCOPY);
```
允许用户缓冲区直接用于网络传输，需要硬件支持。

---

## 零拷贝的应用场景

| 场景                           | 适用技术    |
| ------------------------------ | ----------- |
| 静态文件服务器（Nginx/Apache） | sendfile    |
| 大文件传输                     | mmap/splice |
| 高性能代理                     | splice      |
| 视频流媒体                     | sendfile    |

---

## Nginx 中的零拷贝配置示例
```nginx
http {
    sendfile on;           # 启用sendfile
    tcp_nopush on;         # 配合sendfile使用
    tcp_nodelay on;
}
```

## 注意事项
1. **小文件可能不适用**：零拷贝的优化效果在大文件上更明显
2. **硬件依赖性**：某些零拷贝技术需要特定硬件支持
3. **数据不可修改**：零拷贝通常用于只读场景，因为绕过了用户空间
4. **内存管理**：使用mmap时需注意内存映射的开销

---

## 总结
**sendfile 是零拷贝的重要实现**，它通过减少数据拷贝次数和上下文切换，显著提升了文件传输性能。现代操作系统提供了多种零拷贝机制，开发者应根据具体场景选择合适的技术。

