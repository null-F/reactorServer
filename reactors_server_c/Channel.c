#include "Channel.h"
#include <stdlib.h>

//初始化channel
struct Channel* channelInit(int fd, int events, handleFunc readFunc, handleFunc writeFunc, handleFunc destroyFunc, void* arg)
{
    struct Channel* channel = (struct Channel*)malloc(sizeof(struct Channel));
    channel->fd = fd;
    channel->events = events;
    channel->readCallback = readFunc;
    channel->writeCallback = writeFunc;
    channel->destroyCallback = destroyFunc;
    channel->arg = arg;
    return channel;
}

//修改fd读写事件（检测or不 检测）
void writeEventEnable(struct Channel* channel, bool flag)
{
    if(flag){
        channel->events |= WriteEvent; //设置为检测写事件
    }else{
        channel->events &= (~WriteEvent); //设置为不检测写事件
    }
}

//判断是否需要检测文件描述符号
bool isWriteEventEnable(struct Channel* channel)
{
    return channel->events & WriteEvent;
}