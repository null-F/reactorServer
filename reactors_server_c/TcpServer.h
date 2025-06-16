#pragma once

#include "EventLoop.h"
#include "ThreadPool.h"
#include "Channel.h"
#include "TcpConnection.h"

struct Listener
{
    int lfd;
    unsigned short port;
};

struct TcpServer
{
    struct Listener* listener;
    struct EventLoop* mainLoop;
    struct ThreadPool* threadPool;
    int threadNum;
};

// 初始化监听
struct Listener* listenerInit(unsigned short port);

// 初始化
struct TcpServer* tcpServerInit(unsigned short port,int threadNum);

// 启动服务器（不停检测有无客户端连接）
void tcpServerRun(struct TcpServer* server);