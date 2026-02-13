- 后台进程 又称 守护进程（daemon）它没有控制终端，因此不会意外接收到用户输入		

  守护进程的父进程通常是 init进程（PID为1 的进程）

- 日志系统

- Linux服务器程序的非root身份

- Linux服务器程序的配置，配置文件

- Linux服务器进程的PID文件

- Linux服务器程序 考虑 系统资源和限制

# 7.1日志

## 7.1.1Linux系统日志

Linux使用一个**守护进程**来处理系统日志 -`syslogd`

现代Linux系统通常使用它的升级版本 -`rsyslogd`

`rsyslogd`守护进程既能接受用户进程输出的日志又能接受内核日志。

- 用户进程 是通过调用syslog函数生成系统日志，该函数将日志输出到一个unix 本地域的 socket类型（AF_UNIX)的文件 /dev/log中，rsyslogd则监听该文件以获取用户进程的输出

- 内核日志在老的系统上通过另一个守护进程rklogd来管理，rsyslogd利用二外的模块实现相同的功能

  内核日志由`printk`等函数打印至内核的环状（ring buffer)中。环状缓存的内容直接映射到/proc/kmsg文件中。`rsyslogd`则同通过读取该文件获得系统日志

- rsyslogd守护进程在接收到用户进程或内核输入的日志后，把它们输出到指定的日志文件中

  - 调试信息在：/var/log/debug文件
  - 普通信息在：/var/log/messages文件
  - 内核消息在：/var/log/kern.log文件

  日志信息具体如何分发，可以在rsyslogd的配置文件中设置。

  rsyslogd的主配置文件在：/etc/rsyslog.conf

  rsyslogd的自配置文件则在指定各类日志的目标存储文件

## 7.1.2syslog函数

运用程序使用syslog函数与rsyslogd守护进程通信。

```c
#include <syslog.h>
void syslog(int priority,const char message,..);
```

该函数使用可变参数（第二个参数message和第三个参数...）来结构化输出

- priority参数：设施值与日志级别的 按位或。默认值为LOG_USER

  日志级别的值：

  ```c
  #include <syslog.h>
  #define LOG_EMERG		0//系统不可用
  #define Log_ALERT		1//报警，需要立即采取动作
  #define LOG_CRIT		2//非常严重的错误
  #define LOG_ERR			3//错误
  #define LOG_WARNING		4//警告
  #define LOG_NOTICE		5//通知
  #define LOG_INFO		6//信息
  #define LOG_DEUG		7//调试
  ```

## 7.1.3openlog函数

改变 syslog的默认输出方式的函数

```c
#include <syslog.h>
void openlog(const char* ident,int logopt,int facility);
```

- ident 参数：指定的字符串将被添加到日志消息的日期和时间后，通常被设置为程序的名字

- logopt参数：对后续的syslog调用的行为进行配置，可取以下值的 按位或：

  ```c
  #define LOG_PID 	0X01 //在日志消息中包含程序PID
  #define LOG_CONS 	0X02 //如果消息不能记录到日志文件，则打印至终端
  #define LOG_ODELAY	0X04 //延迟打开日志功能直到第一次调用syslog
  #define LOG_NDELAY	0X08 //不延迟打开日志功能
  ```

- facility参数 ：用来修改syslog函数中的默认设施值

## 7.1.4setlogmask函数

日志的过滤：

简单设置日志掩码，日志级别大于日志掩码的日志信息将被系统忽略。

设置syslog的日志掩码：

```c
#include <syslog.h>
int setlogmask(int maskpri);
```

- maskpri参数 指定日志掩码值。该函数始终会成功。返回调用进程先前的日志掩码值

## 7.1.5closelog函数

关闭日志功能

```c
#include <syslog.h>
void closelgo();
```

# 7.2用户信息

## 7.2.1UID、EUID和GID、EGID

## 7.2.2切换用户

# 7.3进程间关系

## 7.3.1进程组

获取指定进程的PGID

````c
#include <unistd.h>
pid_t getpgid(pid_t pid);
````

设置指定进程的PGID：

```c
#include <unistd.h>
int setpgid(pid_t pid,pid_t pgid);
```

## 7.3.2会话

创建一个会话

```c
#include <unistd.h>
pid_t setsid(void);
```

## 7.3.3 ps查看进程关系

# 7.4 系统资源限制

读取和设置Linux系统资源限制

```c
#include <sys/resource.h>
int getlimit(int resource,struct rlimit* rlim);
int setlimit(int resource,const struct rlimit* rlim);
```

rlim参数是rlimit结构体类型的指针

```
struct rlimit
{
	rlimi_t rlim_cur;
	rlimi_T rlim_max;
}
```

getlimit和setlimit支持的部分资源限制类型

| 资源限制类型     | 含义 |
| ---------------- | ---- |
| RLIMIT_AS        |      |
| RLIMIT_CORE      |      |
| RLIMIT_CPU       |      |
| RLIMIT_DATE      |      |
| RLIMIT_FSIZE     |      |
| RLMIT_NOFILE     |      |
| RLMIT_NPROC      |      |
| PLMIT_SIGPENDING |      |
| PLIMIT_STACK     |      |

# 7.5 改变工作目录和根目录

获取进程当前工作目录和改变进程工作目录

```c
#include <unistd.h>
char* getcwd(char* buf, size_t size);
int chdir(const char* path);
```

改变进程根目录：

```c
#include <unistd.h>
int chroot(const char* path);
```

# 7.6服务器程序后台化

```c
#include <unistd.h>
int daemon(int nochdir,int noclose);
```

- nochdir 参数用来指定是否改变进程工作目录，如果传递0，则工作目录被设置为“`/`”（根目录），否则继续使用当前工作目录
- noclose参数为0 时，标准输入、标准输出和标准错误都被重定向到/dev/null文件，否则依然使用原来设备。