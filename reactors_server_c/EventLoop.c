#include "EventLoop.h"
#include "ChannelMap.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

//初始化函数
struct EventLoop* eventLoopInit()
{
    return eventLoopInitEx(NULL);
}

// 读数据
int readLocalMessage(void* arg)
{
    struct EventLoop* evLoop = (struct EventLoop*)arg;
    char buf[256];
    read(evLoop->socketPair[1], buf, sizeof(buf));
    return 0;
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

    // (已续写)
    int ret = socketpair(AF_UNIX,SOCK_STREAM,0,evLoop->socketPair);
    if(ret == -1){
        perror("socketpair");
        exit(0);
    }
    // 指定规则：evLoop->socketPair[0] 发送数据，evLoop->socketPair[1]接收数据
    struct Channel* channel = channelInit(evLoop->socketPair[1],ReadEvent, 
        readLocalMessage,NULL,NULL,evLoop);
    // channel 添加到任务队列
    eventLoopAddTask(evLoop, channel,ADD);
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
        // (已续写)
        eventLoopProcessTask(evLoop);
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
        eventLoopProcessTask(evLoop);
    }else{
        // 主线程 -- 告诉子线程处理任务队列中的任务
        // 1.子线程在工作 2.子线程被阻塞了：select、poll、epoll
        taskWakeup(evLoop);

    }
    return 0;
}

// 处理任务队列中的任务
int eventLoopProcessTask(struct EventLoop* evLoop)
{
    pthread_mutex_lock(&evLoop->mutex);
    // 取出头节点
    struct ChannelElement* head = evLoop->head;
    while (head != NULL)
    {
        struct Channel* channel = head->channel;
        if(head->type == ADD){
            eventLoopAdd(evLoop,channel);
        }else if (head->type == DELETE)
        {
            eventLoopRemove(evLoop,channel);
        }else if (head->type == MODIFY)
        {
            eventLoopModify(evLoop,channel);
        }
        struct ChannelElement* tmp = head;
        head = head->next;
        free(tmp);    
    }
    evLoop->head = evLoop->tail = NULL;
    pthread_mutex_unlock(&evLoop->mutex);
    return 0;
}

// 处理dispatcher中的任务
// 将任务队列中的任务添加到Dispatcher的文件描述符检测集合中
int eventLoopAdd(struct EventLoop* evLoop,struct Channel* channel) {
    int fd = channel->fd;// 取出文件描述符fd
    struct ChannelMap* channelMap = evLoop->channelMap;// channelMap存储着channel和fd之间的对应关系
    // 需要判断channelMap里边是否有fd 和 channel对应的键值对（其中，文件描述符fd对应的就是数组的下标）
    if(fd >= channelMap->size) {
        // 没有足够的空间存储键值对 fd->channel ==> 扩容
        if(!makeMapRoom(channelMap,fd,sizeof(struct Channel*))) {
            return -1;
        }
    }
    // 找到fd对应的数组元素位置，并存储
    if(channelMap->list[fd] == NULL) {
        channelMap->list[fd] = channel;
        evLoop->dispatcher->add(channel,evLoop);
    } 
    return 0;
}
int eventLoopRemove(struct EventLoop* evLoop,struct Channel* channel) 
{
    int fd = channel->fd;
    struct ChannelMap* channelMap = evLoop->channelMap;
    if(fd >= channelMap->size) {
        return -1;
    }
    int ret = evLoop->dispatcher->remove(channel,evLoop);
    return ret;
}
int eventLoopModify(struct EventLoop* evLoop,struct Channel* channel) 
{
    int fd = channel->fd;
    struct ChannelMap* channelMap = evLoop->channelMap;
    if(fd >= channelMap->size || channelMap->list[fd] == NULL) {
        return -1;
    }
    int ret = evLoop->dispatcher->modify(channel,evLoop);
    return ret;
}

// 释放channel
int destroyChannel(struct EventLoop* evLoop,struct Channel* channel) {
    // 删除 channel 和 fd 的对应关系
    evLoop->channelMap->list[channel->fd] = NULL;
    // 关闭 fd
    close(channel->fd);
    // 释放 channel 内存
    free(channel);
    return 0;
}
