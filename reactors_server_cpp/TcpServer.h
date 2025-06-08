#ifndef _TCPSERVER_H
#define _TCPSERVER_H


class TcpServer
{
private:
    int m_threadNum;
    EventLoop* m_mainLoop;
    ThreadPool* m_threadPool;
    int m_lfd;
    unsigned short m_port;

public:
    TcpServer(unsigned short port, int threadNum);
    void setListen();//初始化监听函数
    void run(); //启动服务器
    static int acceptConnection(void* arg);

};

#endif