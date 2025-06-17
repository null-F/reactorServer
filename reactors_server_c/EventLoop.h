#pragma once
#include "Dispatcher.h"
#include <pthread.h>
#include "Channel.h"
#include <sys/socket.h>

extern struct Dispatcher EpollDispatcher;
extern struct Dispatcher PollDispatcher;
extern struct Dispatcher SelectDispatcher;

struct EventLoop{
    bool isQuit; //开关
    struct Dispatcher* dispatcher;
    void* dispatcherData;

    // 任务队列
    struct ChannelElement* head;
    struct ChannelElement* tail;

    // 用于存储channel的map
    struct ChannelMap* channelMap;

    // 线程ID，Name，mutex
    pthread_t threadID;
    char threadName[32];
    pthread_mutex_t mutex;
    int socketPair[2]; ////存储本地通信的fd 通过socketpair初始化
};
 

// 处理该节点中的channel的方式
enum ElemType{
    ADD,    //添加
    DELETE, //删除
    MODIFY  //修改
};

//定义任务队列节点
struct ChannelElement{
    int type; //如何处理节点中的channel
    struct Channel* channel;
    struct ChannelElement* next;
};

//初始化函数
struct EventLoop* eventLoopInit();
struct EventLoop* eventLoopInitEx(const char* threadName);

//启动反应堆模型
int eventLoopRun(struct EventLoop* evLoop);

//处理被激活的文件描述符
int eventActivate(struct EventLoop* evLoop, int fd, int event);

// 添加任务到任务队列
int eventLoopAddTask(struct EventLoop* evLoop,struct Channel* channel,int type);

// 处理任务队列中的任务
int eventLoopProcessTask(struct EventLoop* evLoop);

// 处理dispatcher中的任务
int eventLoopAdd(struct EventLoop* evLoop,struct Channel* channel);
int eventLoopRemove(struct EventLoop* evLoop,struct Channel* channel);
int eventLoopModify(struct EventLoop* evLoop,struct Channel* channel);

// 释放channel
int destroyChannel(struct EventLoop* evLoop,struct Channel* channel);
