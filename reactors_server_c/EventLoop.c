#include "EventLoop.h"
#include "ChannelMap.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>

//初始化函数
struct EventLoop* eventLoopInit()
{
    return eventLoopInitEx(NULL);
}

struct EventLoop* eventLoopInitEx(const char* threadName)
{
    struct EventLoop* evLoop = (struct EventLoop*)malloc(sizeof(struct EventLoop));
    evLoop->isQuit = false;
    evLoop->dispatcher = &EpollDispatcher;
    evLoop->dispatcherData = evLoop->dispatcher->init();

    // 任务链表
    evLoop->head = evLoop->tail = NULL;

    // 用于存储channel的mao
    evLoop->channelMap = channelMapInit(128);

    evLoop->threadID = pthread_self();

    strcpy(evLoop->threadName,threadName == NULL ? "MainThread" : threadName); // 线程的名字
    pthread_mutex_init(&evLoop->mutex,NULL);

    // .......(续写)

    return evLoop;

}

// 启动反应堆模型
int eventLoopRun(struct EventLoop* evLoop) {
    assert(evLoop != NULL);
    // 取出事件分发和检测模型
    struct Dispatcher* dispatcher = evLoop->dispatcher;
    // 比较线程ID是否正常
    if(evLoop->threadID != pthread_self()) {
        return -1;
    }
    // 循环进行事件处理
    while(!evLoop->isQuit) {
        /*
            dispatch指向的任务函数其实是动态的，由于在初始化的时候指向的是底层的
            IO模型用的是epoll模型，那么dispatcher->dispatch(evLoop,2);
            就是调用epollDispatch，里头的epoll_wait函数会随之被调用，每
            调用一次epoll_wait函数，就会得到一些被激活的文件描述符
            然后通过for循环，对被激活的文件描述符做一系列的处理 
            如果是poll,就是调用pollDispatch，里头的poll函数会随之被调用，每
            调用一次poll函数，就会得到一些被激活的文件描述符
            然后通过for循环，对被激活的文件描述符做一系列的处理 
            如果是select,就是调用selectDispatch，里头的select函数会随之被调用，每
            调用一次select函数，就会得到一些被激活的文件描述符
            然后通过for循环，对被激活的文件描述符做一系列的处理 
        */
        dispatcher->dispatch(evLoop,2); // 超时时长 2s
        // ...(待续写)
    }
    return 0;   
}

//处理被激活的文件描述符
int eventActivate(struct EventLoop* evLoop, int fd, int event)
{
    if(fd < 0 || evLoop == NULL){
        return -1;
    }
    struct Channel* channel = evLoop->channelMap->list[fd];
    assert(channel->fd == fd);
    if(event & ReadEvent && channel->readCallback){
        channel->readCallback(channel->arg);
    }
    if(event & WriteEvent && channel->writeCallback){
        channel->writeCallback(channel->arg);
    }
    return 0;
}

//添加任务到任务队列
int eventLoopAddTask(struct EventLoop* evLoop,struct Channel* channel,int type)
{
    // 加锁，保护共享资源
    pthread_mutex_lock(&evLoop->mutex);
    // 创建新节点然后添加到任务队列中
    struct ChannelElement* node = (struct ChannelElement*)malloc(sizeof(struct ChannelElement));
    node->channel = channel;
    node->type = type;
    node->next = NULL;
    // 链表为空
    if(evLoop->head == NULL){
        evLoop->head = evLoop->tail = node;
    }else{
        evLoop->tail->next = node; //添加
        evLoop->tail = node; //后移
    }
    pthread_mutex_unlock(&evLoop->mutex);
    // 处理节点
    /**
     * 这个描述假设了一个前提条件，就是当前的EventLoop反应堆属于子线程
     * 细节：
     *  1.对于链表节点的添加：可能是当前线程也可能是其他线程（主线程）
     *      1）.修改fd的事件，当前子线程发起，当前子线程处理
     *      2）.添加新的fd（意味着和一个新的客户端建立连接，这是由主线程做的，故添加任务节点这个操作肯定是由主线程做的），
     *          添加任务节点的操作是由主线程发起的
     *  2.不能让主线程处理任务队列里边的节点，需要由当前的子线程去处理
    */

    if(evLoop->threadID == pthread_self()){
        // 当前子线程
        // .........
    }else{
        // 主线程 -- 告诉子线程处理任务队列中的任务
        // 1.子线程在工作 2.子线程被阻塞了：select、poll、epoll
        // ...........

    }
    return 0;
}