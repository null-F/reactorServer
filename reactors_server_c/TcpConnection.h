#pragma once

#include "Channel.h"
#include "EventLoop.h"
#include "Buffer.h"

// #define MSG_SEND_AUTO

struct TcpConnection
{
    struct EventLoop* evLoop;
    struct Channel* channel;
    struct Buffer* readBuf;
    struct Buffer* writeBuf;
    char  name[32];
    // http协议
    struct HttpRequest* request;
    struct HttpResponse* response;
};

// 初始化
struct TcpConnection* tcpConnectionInit(int fd,struct EventLoop* evLoop);

// 接收客户端数据
int processRead(void* arg);

int processWrite(void* arg);

int tcpConnectionDestroy(void* arg);