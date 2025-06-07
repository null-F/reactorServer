#pragma once

//初始化监听
int initListenFd(unsigned short port);

//启动epoll
int epollRun(int lfd);

//和客户端建立连接
int acceptClient(int lfd,int epfd);