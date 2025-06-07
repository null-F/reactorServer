#pragma once

//初始化监听
int initListenFd(unsigned short port);

//启动epoll
int epollRun(int lfd);

//和客户端建立连接
int acceptClient(int lfd,int epfd);


// 主要是接收对端的数据
int recvHttpRequest(int cfd,int epfd);