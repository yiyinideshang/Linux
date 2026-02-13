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

int unblock_connect(const char* ip, int port, int time)
{
    printf("=== Debug: Starting unblock_connect ===\n");
    printf("Debug: Target IP: %s, Port: %d, Timeout: %d seconds\n", ip, port, time);
    
    int ret = 0;
    struct sockaddr_in address;
    
    // 清空结构体
    bzero(&address, sizeof(address));
    
    // 设置地址结构
    address.sin_family = AF_INET;
    printf("Debug: address.sin_family set to: %d (AF_INET=%d)\n", address.sin_family, AF_INET);
    
    // 转换IP地址
    printf("Debug: Converting IP '%s' to binary...\n", ip);
    if (inet_pton(AF_INET, ip, &address.sin_addr) <= 0) {
        printf("Error: inet_pton failed for IP: %s\n", ip);
        return -1;
    }
    
    address.sin_port = htons(port);
    printf("Debug: address.sin_port set to: %d (network byte order: 0x%04x)\n", 
           ntohs(address.sin_port), address.sin_port);
    
    // 打印地址结构体的原始字节
    printf("Debug: Address structure bytes (hex): ");
    unsigned char *ptr = (unsigned char*)&address;
    for (size_t i = 0; i < sizeof(address); i++) {
        printf("%02x ", ptr[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
    
    // 创建socket
    printf("Debug: Creating socket with AF_INET...\n");
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error: socket creation failed");
        return -1;
    }
    printf("Debug: Socket created, fd=%d\n", sockfd);
    
    // 设置为非阻塞
    int fdopt = setnonblocking(sockfd);
    
    // 尝试连接
    printf("Debug: Attempting to connect...\n");
    ret = connect(sockfd, (struct sockaddr*)&address, sizeof(address));
    printf("Debug: connect() returned: %d\n", ret);
    
    if (ret == 0) {
        printf("Debug: Immediate connection success\n");
        fcntl(sockfd, F_SETFL, fdopt);
        return sockfd;
    } else if (errno != EINPROGRESS) {
        printf("Debug: connect failed with errno=%d (%s)\n", errno, strerror(errno));
        close(sockfd);
        return -1;
    }
    
    printf("Debug: Connection in progress (EINPROGRESS)\n");
    
    // 使用select等待连接完成
    fd_set writefds;
    struct timeval timeout;
    
    FD_ZERO(&writefds);
    FD_SET(sockfd, &writefds);
    
    timeout.tv_sec = time;
    timeout.tv_usec = 0;
    
    printf("Debug: Waiting for connection with select, timeout=%d seconds...\n", time);
    
    int maxfd = sockfd + 1;
    int select_ret;
    
    do {
        fd_set tmp_writefds = writefds;
        struct timeval tmp_timeout = timeout;
        
        select_ret = select(maxfd, NULL, &tmp_writefds, NULL, &tmp_timeout);
        printf("Debug: select returned: %d\n", select_ret);
        
        if (select_ret == -1) {
            printf("Debug: select error, errno=%d (%s)\n", errno, strerror(errno));
        }
    } while (select_ret == -1 && errno == EINTR);
    
    if (select_ret == -1) {
        perror("Error: select failed");
        close(sockfd);
        return -1;
    }
    
    if (select_ret == 0) {
        printf("Debug: Connection timeout\n");
        close(sockfd);
        return -1;
    }
    
    printf("Debug: select returned > 0, checking if sockfd is ready...\n");
    
    if (!FD_ISSET(sockfd, &writefds)) {
        printf("Debug: sockfd not in writefds set\n");
        close(sockfd);
        return -1;
    }
    
    // 检查socket错误
    int error = 0;
    socklen_t length = sizeof(error);
    
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &length) < 0) {
        perror("Error: getsockopt failed");
        close(sockfd);
        return -1;
    }
    
    if (error != 0) {
        printf("Debug: Connection failed with error: %d (%s)\n", error, strerror(error));
        close(sockfd);
        return -1;
    }
    
    printf("Debug: Connection successful!\n");
    printf("Debug: Restoring original socket flags\n");
    
    fcntl(sockfd, F_SETFL, fdopt);
    return sockfd;
}

int main(int argc, char* argv[])
{
    // 忽略SIGPIPE信号
    signal(SIGPIPE, SIG_IGN);
    
    if (argc <= 2) {
        printf("Usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    
    printf("=== Main: Starting non-blocking connect test ===\n");
    printf("Main: Connecting to %s:%d\n", ip, port);
    
    int sockfd = unblock_connect(ip, port, 10);
    
    if (sockfd < 0) {
        printf("Main: Connection failed\n");
        return 1;
    }
    
    printf("Main: Connection established, sockfd=%d\n", sockfd);
    printf("Main: Closing socket...\n");
    
    close(sockfd);
    return 0;
}