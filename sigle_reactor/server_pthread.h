#pragma once

//初始化监听
int initListenFd(unsigned short port);

//启动epoll
int epollRun(int lfd);

//和客户端建立连接
int acceptClient(int lfd,int epfd);

// 主要是接收对端的数据
int recvHttpRequest(int cfd,int epfd);

// 解析请求行
int parseRequestLine(const char* line,int cfd);

// 发送响应头(状态行 + 响应头)
int sendHeadMsg(int cfd,int status,const char* descr,const char* type,int length);
const char* getFileType(const char* name);

// 发送文件
int sendFile(const char* fileName,int cfd);

// 发送目录
int sendDir(const char* dirName,int cfd); 

// 解决浏览器无法访问带特殊字符的文件得到问题
int hexToDec(char c);
void decodeMsg(char* to,char* from);