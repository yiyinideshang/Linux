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
#include <sys/un.h>

int main(int argc, char* argv[])
{
    if(argc <= 2) {
        printf("usage:%s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    
    // 创建 socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }
    
    // 设置地址结构
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    
    // 打印地址结构内容
    printf("Before inet_pton:\n");
    printf("  sin_family: %d (AF_INET=%d)\n", address.sin_family, AF_INET);
    printf("  sin_port: %d (0x%04x)\n", ntohs(address.sin_port), address.sin_port);
    printf("  sin_addr: 0x%08x\n", address.sin_addr.s_addr);
    
    if (inet_pton(AF_INET, ip, &address.sin_addr) <= 0) {
        printf("inet_pton error for %s\n", ip);
        close(sockfd);
        return 1;
    }
    
    printf("After inet_pton:\n");
    printf("  sin_family: %d\n", address.sin_family);
    printf("  sin_port: %d (0x%04x)\n", ntohs(address.sin_port), address.sin_port);
    printf("  sin_addr: 0x%08x\n", address.sin_addr.s_addr);
    
    // 打印原始字节
    printf("Address structure bytes (hex):\n  ");
    unsigned char *p = (unsigned char*)&address;
    for (int i = 0; i < sizeof(address); i++) {
        printf("%02x ", p[i]);
        if ((i + 1) % 8 == 0) printf("\n  ");
    }
    printf("\n");
    
    // 直接调用 connect（阻塞模式）
    printf("\nCalling connect()...\n");
    int ret = connect(sockfd, (struct sockaddr*)&address, sizeof(address));
    
    if (ret == 0) {
        printf("connect() returned 0 (success)\n");
    } else {
        printf("connect() returned -1, errno=%d (%s)\n", errno, strerror(errno));
    }
    
    close(sockfd);
    return 0;
}