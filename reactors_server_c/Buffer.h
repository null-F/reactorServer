#pragma once

struct Buffer
{
    // 指向内存的指针
    char* data;
    int capacity;
    int readPos;
    int writePos;
};

// 初始化
struct Buffer* bufferInit(int size);

// 销毁
void bufferDestroy(struct Buffer* buf);

// 得到剩余的可写的内存容量
int bufferWriteableSize(struct Buffer* buf);

// 已写数据内存（未读）的大小 --- 得到剩余的可读的内存容量
int bufferReadableSize(struct Buffer* buf);

// 扩容
void bufferExtendRoom(struct Buffer* buf, int size);

// 写内存 1.直接写 
int bufferAppendData(struct Buffer* buf, const char* data, int size); 
int bufferAppendString(struct Buffer* buf, const char* data);

// 写内存 2.接收套接字数据
int bufferSocketRead(struct Buffer* buf,int fd);

// 发送数据
int bufferSendData(struct Buffer* buf,int socket);