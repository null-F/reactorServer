#include "server.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

int initListenFd(unsigned short port)
{
    //1.创建监听套接字
    int lfd = socket(AF_INET,SOCK_STREAM,0);
    if(lfd == -1){
        perror("socket");
        return -1;
    }
    //2.设置端口号复用 （可选）
    int opt = 1;
    int ret = setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    if(ret == -1){
        perror("setsockport");
        return -1;
    }
    //3.绑定
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port); // 主机字节序转为网络字节序
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(lfd,(struct sockaddr*)&addr,sizeof(addr));
    if(ret == -1){
        perror("bind");
        return -1;
    }
    return lfd;
}

int epollRun(int lfd)
{
    //1.创建epoll实例
    int epfd = epoll_create(1);
    if(epfd == -1){
        perror("epoll_create");
        return -1;
    }
    // 2.添加监听fd lfd上树 对于监听的描述符来说只需要看一下有没有新的客户端连接
    struct epoll_event ev;
    ev.data.fd = lfd;
    ev.events = EPOLLIN; // 委托epoll(内核)帮我们检测lfd的读事件
    int ret = epoll_ctl(epfd,EPOLL_CTL_ADD,lfd,&ev);
    if(ret == -1){
        perror("epoll_ctl");
        return -1;
    }
    // 3.检测
    struct epoll_event evs[1024];
    int size = sizeof(evs) / sizeof(evs[0]);
    while (1)
    {
        int num = epoll_wait(epfd,evs,size,-1); //-1表示一直阻塞， 返回值是已经就绪的文件描述符的个数
        if(num == -1){
            perror("epoll_wait");
            return -1;
        }
        for(int i = 0; i < num; ++i){
            int fd = evs[i].data.fd;
            if(fd == lfd){
                //建立连接
                acceptClient(lfd,epfd);
            }else{
                //接收对端数据
                recvHttpRequest(fd,epfd);
            }
        }
    }
    return 0;   
}

// 接受新连接，把得到的新的文件描述符cfd也添加到epoll树上
int acceptClient(int lfd,int epfd)
{
    // 1. 创建套接字，；建立连接
    int cfd = accept(lfd,NULL,NULL); //后两个参数设置为NULL表示不关注客户端IP地址以及端口号信息
    if(cfd == -1){
        perror("accept");
        return -1;
    }
    // 2. 添加到epoll中
    int flag = fcntl(cfd,F_GETFL); //设置为非阻塞模式，主要针对于读缓存区数据循环接收
    flag |= O_NONBLOCK; //非阻塞模式
    fcntl(cfd,F_SETFL,flag);
    // 3. 将cfd添加到epoll中
    struct epoll_event ev;
    ev.data.fd = cfd;
    ev.events = EPOLLIN | EPOLLET; //设置为边沿触发模式
    int ret = epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&ev);
    if(ret == -1){
        perror("epoll_ctl");
        return -1;
    }
    return 0;
}

int recvHttpRequest(int cfd,int epfd)
{
    // 有了存储数据的内存之后，接下来就是读数据
    // 注意：前面已经把用于通信的文件描述符的事件改成了边缘非阻塞
    // 如果是边缘模式epoll检测到文件描述符对应的读事件之后，只会给我们通知一次
    // 因此需要得到这个通知之后，
    printf("开始接收数据.......\n");
    int len = 0 , total = 0;
    char buf[4096] = {0};
    char temp[1024] = {0};
    while (len = recv(cfd,temp,sizeof(temp),0) > 0)
    {
        if(total + len < sizeof(buf))
            memcpy(buf+total,temp,len);
        total += len;
    }
    //判断数据是否接收完毕
    if(len == -1 && errno == EAGAIN){
        // 说明服务器已经把客户端发过来的请求数据接收完毕了
        // 解析请求行
        char* pt = strstr(buf,"\r\n");
        int reqLen = pt - buf;
        buf[reqLen] = '\0';
        parseRequestLine(buf,cfd);
    }
    else if(len == 0){
        //说明客户端断开连接
        int ret = epoll_ctl(epfd,EPOLL_CTL_DEL,cfd,NULL);
        if(ret == -1){
            perror("epoll_ctl");
            return -1;
        }
        close(cfd);
    }else{
        perror("recv");
    }
    return 0;
}