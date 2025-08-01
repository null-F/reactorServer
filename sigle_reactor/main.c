#include <stdio.h>
#include <stdlib.h>
// #include "server.h"
#include "server_pthread.h"
#include "unistd.h"

int main(int argc, char const *argv[])
{
    if(argc < 3){
        printf("./a.out port path\n");
        return -1;
    }
    unsigned short port = atoi(argv[1]);
    //切换服务器的工作路径
    chdir(argv[2]);
    //初始化用于监听的套接字
    int lfd = initListenFd(port);
    //启动服务器程序
    epollRun(lfd);
    return 0;
}
