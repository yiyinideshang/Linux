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

    ret = select(sockfd+1,NULL,&writefds,NULL,&timeout);//监控sockfd描述符
    if(ret <= 0)
    {
        printf("select超时或错误\n");
        close(sockfd);
        return -1;
    }

    //防御性编程
    //即使 select 返回结果：ret > 0，就绪的也不一定是 sockfd（虽然目前集合中只有它）。
    if(!FD_ISSET(sockfd,&writefds))//若未就绪，说明内核状态异常或编程错误，不应继续。❌ 返回失败。
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