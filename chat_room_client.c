#define _GNU_SOURCE 1//启用 GNU 扩展，确保 poll.h 中 pollfd 类型和 splice 等函数可用。  
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <poll.h>
#include <fcntl.h>
#include <libgen.h>
#define BUFFER_SIZE 64

int main(int argc,char* argv[])
{
    if(argc <= 2)
    {
        printf("usage:%s ip_address port_number\n",basename(argv[0]));//获取程序名,需要 <libgen.h>
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    //创建TCP socket，连接到服务器
    struct sockaddr_in server_address;
    bzero(&server_address,sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET,ip,&server_address.sin_addr);
    server_address.sin_port = htons(port);

    int sockfd = socket(PF_INET,SOCK_STREAM,0);
    assert(sockfd >= 0);
    if(connect(sockfd,(struct sockaddr*)&server_address,sizeof(server_address))<0)
    {
        printf("connection failed\n");
        close(sockfd);
        return 1;
    }

    struct pollfd fds[2];
    fds[0].fd = 0;//监听标准输入 fd=0
    fds[0].events = POLLIN;//数据可读
    fds[0].revents = 0;

    fds[1].fd = sockfd;//监听连接服务器的套接字 sockfd
    fds[1].events = POLLIN | POLLRDHUP;///socket 可读或检测对端关闭连接
    fds[1].revents = 0;

    char read_buf[BUFFER_SIZE];
    int pipefd[2];
    int ret = pipe(pipefd);
    assert(ret != 1);

    while(1)
    {
        ret = poll(fds,2,-1);
        if(ret < 0)
        {
            printf("poll failure\n");
        }

        if(fds[1].revents & POLLRDHUP)//对端关闭
        {
            printf("server close the connection\n");
            break;
        }
        else if(fds[1].revents & POLLIN)//socket可读
        {
            memset(read_buf,'\0',BUFFER_SIZE);
            recv(fds[1].fd,read_buf,BUFFER_SIZE-1,0);
            printf("%s\n",read_buf);
        }
        if(fds[0].revents & POLLIN)//标准输入可读
        {
            ret = splice(0,NULL,pipefd[1],NULL,32768,SPLICE_F_MORE | SPLICE_F_MOVE);//从标准输入读取最多32768字节到管道写端
            ret = splice(pipefd[0],NULL,sockfd,NULL,32768,SPLICE_F_MORE | SPLICE_F_MOVE);//从管道读端读取到数据发送到socket
        }
    }
    close(sockfd);
    return 0;

}