#pragma once
#include <stdbool.h>

//定义函数指针
typedef int(*handleFunc)(void* arg);

//定义文件描述符的读写事件
enum FDEvent{
    TimeOut = 0x01;
    ReadEvent = 0x02;
    WriteEvent = 0x04;
};

struct Channel{
    int fd; //文件描述符
    int events; //事件
    handleFunc readCallback; //读回调
    handleFunc writeCallback; //写回调
    void* arg; //回调函数参数
};

//初始化channel
struct Channel* channelInit(int fd, int events, handleFunc readFunc, handleFunc writeFunc,void* arg);

//修改fd读写事件（检测or不 检测）
void writeEventEnable(struct Channel* Channel, bool flag);

//判断是否需要检测文件描述符号
bool isWriteEventEnable(struct Channel* channel);
