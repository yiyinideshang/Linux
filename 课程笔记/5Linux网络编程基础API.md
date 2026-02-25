# Linux网络API：

- socket地址API
- socket基础API
- 网络信息API

# 5.1 socket地址API

==参见D:\Typora\typora_work\5网络编程\1.1socket套接字\socket.md==

## 5.1.1 主机字节序和网络字节序

- 现代PC大多数采用**小端字节序**，因此小端字节序又被称为**主机字节序**
- 为了解决两台使用不同字节序的主机之间直接传递导致接收端错误解释，采用一下方法：
  - 发送端总是将要发送的数据转换为 大端字节序
  - 接受端总是接受 这个大端字节序，在根据自己采用的字节序决定是否再进行转换
- **大端字节序**又称为 **网络字节序**
- 即使是同一台机子的两个进程也要考虑字节序问题，如`java虚拟机采用大端字节序`
- 输入：32位整数，输出：32位整数

```c++
#include <netinet/in.h>
//host->net long net short
unsigned long int htonl(usingned long int hostlong);//主机字节序-》网络字节序
unsigned short int htons(usingned short int hostshort);//主机字节序-》网络字节序
//net->host long host short
unsigned long int ntohl(usingned long int netlong);//网络字节序-》主机字节序
unsigned short int ntohs(usingned short int netshort)//网络字节序-》主机字节序
```

- 长整形`long int`函数常用来转换IP地址
- 短整形`long short`函数常用来转换端口号

## 5.1.2 通用socket地址

```c++
struct sockaddr//16字节
{
	sa_family_t sin_family; // 指定地址族，2字节
	char sa_data[14];//14字节
};
```

## 新的通用socket地址

```c++
struct sockaddr_storage
{
	sa_family_t sin_family;
	unsigned long int __ss_align;
	char __ss_padding(128-sizeof(__sss_align));
}
```

## 5.1.3 专用socket地址

```c++
struct sockadd_un
{
    sa_family_t sin_family;//指定地址族  2字节
    char sun_path[108];//文件路径名
}

struct sockaddr_in// 16字节 (IPv4套接字地址)
{
	sa_family_t sin_family; // 指定地址族 2字节
	u_int16_t sin_port; // 端口号 2字节
	struct in_addr sin_addr;// IP地址 4字节
	char sin_zero[8]; // 填充8字节，为了和其他协议簇地址结构体大小一样
};
struct in_addr
{
    u_int32_t s_addr;//32位ipv4地址，用网络字节序表示
}

struct sockaddr_in6 {    // 28字节 (IPv6套接字地址)
    sa_family_t     sin6_family;   // 2字节
    in_port_t       sin6_port;     // 2字节
    uint32_t        sin6_flowinfo; // 4字节
    struct in6_addr sin6_addr;     // 16字节
    uint32_t        sin6_scope_id; // 4字节

};
struct in6_addr
{
    ussigned char sa_addr[16];//ipv4地址，用网络字节序表示
}

struct sockaddr_in sock_info;
sock_info.sin_family = AF_INET; // 指定为IPV4
sock_info.sin_port = htons(6666); //指定为6666端口
sock_info.sin_addr.s_addr = inet_addr("192.168.31.1"); // 绑定ip地址 法1
// //in_addr_t的函数原型:in_addr_t inet_addr(const char* strptr);将字符串strptr转换后的结果用 返回值接收

// inet_aton("192.168.31.1",&sock_info.sin_addr);//绑定ip地址 法2
// //inet_aton的函数原型：int inet_aton(const char* cp,struct in_addr* inp);将字符串cp 转换后的结果储存到参数inp指向的结构体中（这个结构体存储的就是ipv4地址的网络字节序）
//inet_pton(AF_INET,"192.168.31.1"，&sock_info.sin_addr)
```

![v2-48f56688737085cf83d132b6f0462ad3_r](D:\Typora\typora_work\5网络编程\2TCP协议\v2-48f56688737085cf83d132b6f0462ad3_r.jpg)

## 5.1.4 IP地址转换函数

==73页==

```c++
#include <arpa/inet.h>
in_addr_t inet_addr(const char* strptr);//IPv4地址字符串 -》网络字节序
int inet_aton(const char* cp,struct in_addr* inp);//网络字节序 -》Iv4P地址字符串
//struct in_addr
//{
//    u_int32_t s_addr;//32位ipv4地址，用网络字节序表示
//}

char* inet_ntoa(struct in_addr in);
```

- `inet_addr`函数将用点分十进制字符串表示的IPV4地址转换为用 网络字节序整数表示的IPV4地址(**用返回值来接收**)，失败时返回`INADDR NONE` 
  - 输入：字符串
  - 输出：32位整数(网络字节序) 

- `inet_aton`函数完成和`inet_addr`相同的功能，但是它将转换后的结果储存到参数inp指向的结构体中(**用指向结构体的指针来接收**，这个结构体存储的就是ipv4地址的网络字节序)，成功返回1，失败返回0
  - 输入：字符串
  - 输出：结构体，指向该结构体的指针内部存放转换后的 32位整数(网络字节序)
- `inet_ntoa`函数将用网络字节序整数表示的IPV4地址转换为用点分十进制字符串表示的IPV4地址。指的注意的是：它的函数内部用一个静态变量存储转换结果，该函数的 返回值指向这个 静态内存，因此`inet_ntoa`是不可重入的，即它的结果始终是一个值。
  - 输入:字符串
  - 输出：指向存储  32位整数(网络字节序) 的 静态变量



- #### 以下这两对新函数也能完成前面3个函数同样的功能，并且同时适用IPV4和IPV6地址

```c++
#include <arpa/inet.h>
int inet_pton(int af,const char* src,void* dst);//IP地址字符串 -》网络字节序
const char* inet_ntop(int af,const void* src,char* dst,socklen_t cnt);//网络字节序 -》IP地址字符串
```

- ## 参数

  - 其中`src`参数表示**用字符串表示的IP地址**

  - `af`参数表示**指定地址族**

  - `dst`参数表示转换后的结果存储与`dst`指向的内存

  - `cnt`参数用来指定目标存储单元的大小

    ```c++
    #include <netinet/in.h>
    #define INET_ADDRSTRLEN 16;//ipv4
    #define INET6_ADDRSTRLEN 46;//ipv6
    ```

  - `inet_pton` 成功返回1，失败返回0，并设置`errno`

  - `inet_ntop`成功返回目标存储单元的地址，失败返回NULL并设置`errno`

# 5.2 创建socket

==74页==

`socket` 函数用于创建一个新的套接字，它是网络编程中最基础的函数之一。其原型如下：

```c
#include <sys/socket.h>
int socket(int domain, int type, int protocol);
```

### 参数说明

1. **`domain`**（协议域/地址族）
   指定套接字使用的通信协议族，常见的取值有：
   - **AF_INET**：`IPv4` 协议
   - **AF_INET6**：`IPv6` 协议
   - **AF_UNIX** 或 **AF_LOCAL**：`Unix` 域协议（用于同一台机器上的进程间通信）
   - **AF_PACKET**：底层包接口（`Linux` 特有）
2. **`type`**（套接字类型）
   指定套接字的通信语义，常用的取值有：
   - **SOCK_STREAM**：提供有序、可靠、双向、基于连接的字节流（对应 TCP）
   - **SOCK_DGRAM**：提供不可靠、无连接的数据报（对应 UDP）
   - **SOCK_RAW**：原始套接字，允许直接访问底层协议（需要特权）
   - **SOCK_SEQPACKET**：有序、可靠、基于连接的数据报（保留消息边界）
3. **`protocol`**（协议）
   指定实际使用的传输协议。通常设置为 `0`，表示根据 `domain` 和 `type` 的组合选择默认协议。例如：
   - 对于 `AF_INET` + `SOCK_STREAM`，默认协议是 `IPPROTO_TCP`（TCP）
   - 对于 `AF_INET` + `SOCK_DGRAM`，默认协议是 `IPPROTO_UDP`（UDP）
     如果需要显式指定，可以使用如 `IPPROTO_TCP`、`IPPROTO_UDP`、`IPPROTO_SCTP` 等常量。

### 返回值

- **成功**：返回一个非负整数的**文件描述符**，代表新创建的套接字。
- **失败**：返回 `-1`，并设置 `errno` 以指示错误原因（如 `EPROTONOSUPPORT`、`EMFILE` 等）。
  - ==关于errno错误码见：D:\Typora\typora_work\Linux高性能服务器编程\9IO复用.md==

# 5.3 命名socket

==75页==

`bind`==参见D:\Typora\typora_work\5网络编程\2TCP协议\2TCP协议.md==

常见`errno`：**EACCES**(被绑定的地址是受保护的地址，仅超级用户能够访问)和**EADDRINUSE**(被绑定的地址正在使用中)

# 5.4 监听socket

==76页==

`listen`==参见D:\Typora\typora_work\5网络编程\2TCP协议\2TCP协议.md==

`atoi()`——将**字符串转换为整数**

`assert()`——**调试辅助工具**，用于在程序运行时检查假设条件是否成立。如果条件成立，程序继续，如果条件为假，程序会立即终止并输出错误信息。

*未完成连接队列*（`SYN_RCVD` 状态）和*已完成连接队列*（`ESTABLISHED` 状态）。

==三次握手和四次挥手参见：60-62页；知乎`https://zhuanlan.zhihu.com/p/670040600`==

# 5.5 接受连接

==78页==

`accpet`==参见D:\Typora\typora_work\5网络编程\2TCP协议\2TCP协议.md==

`bzero`、`memset`——将 `sockaddr_in` 结构体清零

# 5.6 发起连接

==80页==

`connect`==参见D:\Typora\typora_work\5网络编程\2TCP协议\2TCP协议.md==

常见`errno`：**ECONNREFUSED**(目标端口不存在，连接被拒绝)和**ETIMEDOUT**(连接超时)

# 5.7 关闭连接

==80-81页==

`close`/`shutdown`==参见D:\Typora\typora_work\5网络编程\2TCP协议\2TCP协议.md==

# 5.8 数据读写

==81页==

`read`/`write`==参见：D:\Typora\typora_work\5网络编程\1.1socket套接字\socket.md==

## 5.8.1 TCP数据读写

==参见D:\Typora\typora_work\5网络编程\2TCP协议\2TCP协议.md==

- 用于TCP流数据读写的系统调用是：`recv`/`send`

```c
#include <sys/types.h>
#include <sys/socket.h>
ssize_t recv(int sockfd,void* buf,size_t len,int flags);//读取	内核空间 → 用户空间（用户区可写）
ssize_t send(int sockfd,const void *buf,size_t len,int flags);//发送	 用户空间 → 内核空间（用户区只读）
```



**flags参数**的常用值：**`MSG_OOB`**：表示发送或接收紧急数据

> 代码清单5-6 发送带外数据——客户端发送数据到服务器

```c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#incldue <stdlib.h>

int main(int argc,char* argv[])
{
	if(argc<2)
	{
		printf("usage:%s ip_address port_number\n",basename(argv[0]));
		return -1;
	}
	const char* ip = argv[1];
	int port = atoi(argv[2]);
	
	struct sockaddr_in server_address;
	bzero(&server_address,sizeof(server_address));
	server_address.sin_family = AF_INET;
	inet_pton(AP_INET,ip,&server_address.sin_addr);
	server_address.sin_port = htons(port);
	
	int sockfd = socket(PF_INET,SOCK_STREAM,0);
	assert(sockfd>=0);
	if(connect(sockfd,(struct sockaddr*)&server_address,sizeof(server_address))<0)
		printf("coonnection failed\n");
	else
	{
		const char* oob_data = "abc";
		const char* normal_data = "123";
		send(sockfd,normal_data,strlen(normal_data),0);
		send(sockfd,oob_data,strlen(oob_data),0);
        send(sockfd,normal_data,strlen(normal_data),0);
    }
    close(sockfd);
    return 0;
}
```



> 代码清单 5-7 接收带外数据——服务器接收来自客户端的数据

```
```



## 5.8.2 UDP数据读写

==参见D:\Typora\typora_work\5网络编程\3UDP协议及网络协议\3UDP及通信协议.md==

- 用于UDP数据报的系统调用是：`recvfrom`/`sendto`

  只是比`recv`和`send`多了两个参数。

## 5.8.3 通用数据读写函数

==86页==

既能用于`TCP`流数据，也能用于`UDP`数据报

```c
#include <sys/socket.h>
ssize_t recvmsg(int sockfd,struct msghdr* msg,int flags);
ssize_t sendmsg(Int sockfd,struct msghdr* mgs,int flags);
```

# 5.9 带外标记

**带外数据**：==参看50-51页	3.8带外数据==

- 带外（Out Of Band,`OOB`)数据(紧急数据)，用于迅速通告对方本端发生的重要事件。比普通数据（带内数据）有更高的优先级，它应该总是立即被发送，而不论发送缓冲区中是否有排队等待发送的普通数据。

==87页==

代码清单5-7演示了TCP带外数据的接收方法。但在实际应用中，我们通常无法预期带外数据何时到来。好在Linux内核检测到TCP紧急标志时，将通知应用程序有带外数据需要接收。内核通知应用程序带外数据到达有两种常见的方式：

I/O复用产生的异常事件和SIGURG信号。

但是即使应用程序得到了有带外数据需要接收的通知，还需要知道带外数据在数据流中的具体位置，才能准确接收带外数据。

这正是一下系统调用的作用：

```c
#include <sys/socket.h>
int sockatmark(int sockfd);
```

sockatmark判断sockfd是否处于带外标记，即下一个被读取的数据是否是带外数据。如果是，sockatmark返回1，此时我们就可以利用带MSG_OOB标记的recv调用来接收带外数据，如果不是，则sockatmark返回0。