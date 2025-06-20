#include "TcpServer.h"
#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>

// 初始化监听
struct Listener* listenerInit(unsigned short port){
    //创建一个Listener实例
    struct Listener* listener = (struct Listener*)malloc(sizeof(struct Listener));
    //1.创建监听文件描述符
    int lfd = socket(AF_INET,SOCK_STREAM,0);
    if(lfd == -1){
        perror("socket");
        return NULL;
    }
    //2.设置端口号复用
    int opt = 1;
    int ret = setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    if(ret == -1){
        perror("setsockport");
        return NULL;
    }  
    //3.绑定
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(lfd,(struct sockaddr*)&addr,sizeof(addr));
    if(ret == -1){
        perror("bind");
        return NULL;
    }
    //4.设置监听
    ret = listen(lfd,128);
    if(ret == -1){
        perror("listen");
        return NULL;
    }
    listener->lfd = lfd;
    listener->port = port;
    return listener;
}

// 初始化
struct TcpServer* tcpServerInit(unsigned short port,int threadNum)
{
    struct TcpServer* tcp = (struct TcpServer*)malloc(sizeof(struct TcpServer));
    tcp->listener = listenerInit(port);
    tcp->mainLoop = eventLoopInit();
    tcp->threadPool = threadPoolInit(tcp->mainLoop,threadNum);
    tcp->threadNum = threadNum;
    return tcp;
}

int acceptConnection(void* arg) {
    struct TcpServer* server = (struct TcpServer*)arg;
    // 和客户端建立连接
    int cfd = accept(server->listener->lfd,NULL,NULL);
    if(cfd == -1) {
        perror("accept");
        return -1;
    }
    // 从线程池中去取出一个子线程的反应堆实例，去处理这个cfd
    struct EventLoop* evLoop = takeWorkerEventLoop(server->threadPool);
    // 将cfd放到 TcpConnection中处理
    tcpConnectionInit(cfd, evLoop);
    return 0;
}

// 启动服务器（不停检测有无客户端连接）
void tcpServerRun(struct TcpServer* server)
{
    //启动线程池
    threadPoolRun(server->threadPool);
    //初始化一个channel实例
    struct Channel* channel = channelInit(server->listener->lfd,ReadEvent,acceptConnection,NULL,NULL,server);
    //添加检测的任务
    eventLoopAddTask(server->mainLoop,channel,ADD);
    //启动反应堆模型
    eventLoopRun(server->mainLoop);
}