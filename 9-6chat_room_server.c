#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>      // 添加 memset
#include <poll.h>
#include <libgen.h>       // 添加 basename

#define USER_LIMIT 5
#define BUFFER_SIZE 64
#define FD_LIMIT 65535

struct client_data {
    struct sockaddr_in address;
    char* write_buf;        // 指向待发送数据的指针（简化实现）
    char buf[BUFFER_SIZE];  // 接收缓冲区
};

// 设置非阻塞 I/O
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

int main(int argc, char* argv[]) {
    if (argc <= 2) {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);                 // 修正变量名 prot -> port

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_family = AF_INET;
    address.sin_port = htons(port);           // 修正函数名 htond -> htons

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);                         // 修正断言条件

    // 动态分配客户端数据数组，避免栈溢出
    struct client_data* users = (struct client_data*)malloc(sizeof(struct client_data) * FD_LIMIT);
    assert(users != NULL);

    struct pollfd fds[USER_LIMIT + 1];
    int user_counter = 0;

    // 初始化 pollfd 数组（除监听 fd 外）
    for (int i = 1; i <= USER_LIMIT; ++i) {
        fds[i].fd = -1;
        fds[i].events = 0;
        fds[i].revents = 0;
    }

    // 设置监听 fd
    fds[0].fd = listenfd;
    fds[0].events = POLLIN | POLLERR;
    fds[0].revents = 0;

    printf("Server started, listening on %s:%d\n", ip, port);

    while (1) {
        // 轮询所有描述符（监听 + 已连接客户端）
        ret = poll(fds, user_counter + 1, -1);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;           // 被信号中断，重新 poll
            }
            printf("poll failure: %s\n", strerror(errno));
            break;
        }

        for (int i = 0; i < user_counter + 1; ++i) {
            // 跳过无效 fd
            if (fds[i].fd < 0) continue;

            // 处理监听 socket 上的新连接
            if (fds[i].fd == listenfd && (fds[i].revents & POLLIN)) {
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlen);

                if (connfd < 0) {
                    printf("accept error: %s\n", strerror(errno));
                    continue;
                }

                // 检查是否已达最大用户数
                if (user_counter >= USER_LIMIT) {
                    const char* info = "too many users\n";
                    printf("%s", info);
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }

                // 新连接加入
                user_counter++;
                users[connfd].address = client_address;
                setnonblocking(connfd);

                fds[user_counter].fd = connfd;
                fds[user_counter].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_counter].revents = 0;

                printf("comes a new user (%s:%d), now have %d users\n",
                       inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port), user_counter);
            }
            // 处理 POLLERR 错误
            else if (fds[i].revents & POLLERR) {
                printf("get an error from %d\n", fds[i].fd);
                int error = 0;
                socklen_t len = sizeof(error);
                if (getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
                    printf("socket error: %s\n", strerror(error));
                }
                // 关闭该连接并清理
                close(fds[i].fd);
                users[fds[i].fd].write_buf = NULL;   // 清理指针（可选）
                // 将最后一个有效用户移到当前位置，保持数组紧凑
                if (i != user_counter) {
                    fds[i] = fds[user_counter];
                    users[fds[i].fd].write_buf = NULL; // 重置被移动的写指针
                }
                fds[user_counter].fd = -1;
                user_counter--;
                i--;  // 继续检查当前位置（原最后一个已移过来）
            }
            // 处理客户端关闭连接 (POLLRDHUP)
            else if (fds[i].revents & POLLRDHUP) {
                printf("a client (fd=%d) left\n", fds[i].fd);
                close(fds[i].fd);
                users[fds[i].fd].write_buf = NULL;
                // 移动最后一个有效用户到当前位置
                if (i != user_counter) {
                    fds[i] = fds[user_counter];
                    users[fds[i].fd].write_buf = NULL;
                }
                fds[user_counter].fd = -1;
                user_counter--;
                i--;
            }
            // 处理可读事件（客户端发来数据）
            else if (fds[i].revents & POLLIN) {
                int connfd = fds[i].fd;
                memset(users[connfd].buf, 0, BUFFER_SIZE);
                int recv_len = recv(connfd, users[connfd].buf, BUFFER_SIZE - 1, 0);
                if (recv_len < 0) {
                    if (errno != EAGAIN) {
                        // 致命错误，关闭连接
                        printf("recv error from %d: %s\n", connfd, strerror(errno));
                        close(connfd);
                        users[connfd].write_buf = NULL;
                        if (i != user_counter) {
                            fds[i] = fds[user_counter];
                            users[fds[i].fd].write_buf = NULL;
                        }
                        fds[user_counter].fd = -1;
                        user_counter--;
                        i--;
                    }
                } else if (recv_len == 0) {
                    // 客户端正常关闭（相当于 POLLRDHUP，但有些平台可能不触发）
                    printf("client (fd=%d) closed connection\n", connfd);
                    close(connfd);
                    users[connfd].write_buf = NULL;
                    if (i != user_counter) {
                        fds[i] = fds[user_counter];
                        users[fds[i].fd].write_buf = NULL;
                    }
                    fds[user_counter].fd = -1;
                    user_counter--;
                    i--;
                } else {
                    // 成功接收数据，准备广播给其他客户端
                    printf("received %d bytes from %d: %s\n", recv_len, connfd, users[connfd].buf);
                    // 遍历所有其他客户端，设置 POLLOUT 事件，并指向发送缓冲区
                    for (int j = 1; j <= user_counter; ++j) {
                        if (fds[j].fd == connfd) continue;  // 跳过自己
                        // 设置写事件，并指向发送缓冲区
                        fds[j].events |= POLLOUT;
                        users[fds[j].fd].write_buf = users[connfd].buf;  // 指向发送者的数据
                    }
                    // 注意：这里简化了，如果多个客户端同时发送，write_buf 会被覆盖。
                    // 实际项目应使用队列或复制数据，此处仅作演示。
                }
            }
            // 处理可写事件（向客户端发送数据）
            else if (fds[i].revents & POLLOUT) {
                int connfd = fds[i].fd;
                if (!users[connfd].write_buf) {
                    // 没有待发送数据，关闭写事件
                    fds[i].events &= ~POLLOUT;
                    continue;
                }
                int len = strlen(users[connfd].write_buf);
                int sent = send(connfd, users[connfd].write_buf, len, 0);
                if (sent < 0) {
                    if (errno != EAGAIN) {
                        printf("send error to %d: %s\n", connfd, strerror(errno));
                        close(connfd);
                        users[connfd].write_buf = NULL;
                        if (i != user_counter) {
                            fds[i] = fds[user_counter];
                            users[fds[i].fd].write_buf = NULL;
                        }
                        fds[user_counter].fd = -1;
                        user_counter--;
                        i--;
                    }
                    // EAGAIN 忽略，下次再发
                } else if (sent < len) {
                    // 部分发送，移动指针并保留剩余数据（简化：不处理，丢失剩余）
                    // 实际应记录偏移量，此处为简化仅发送一次
                    printf("partial send to %d, sent %d/%d\n", connfd, sent, len);
                    users[connfd].write_buf += sent;  // 指针移动，但源缓冲区可能被覆盖，不安全！
                    // 为了演示，我们假设不会出现部分发送，或仅发送一次。
                    // 更好的做法：使用发送队列。
                    // 此处保持 POLLOUT 继续尝试，但可能出错。
                } else {
                    // 发送完成
                    users[connfd].write_buf = NULL;
                    fds[i].events &= ~POLLOUT;   // 清除写事件
                }
            }
        } // end for
    } // end while

    // 清理资源
    close(listenfd);
    free(users);
    return 0;
}