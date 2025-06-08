#include "server_pthread.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <assert.h>
#include <dirent.h>
#include <sys/sendfile.h>

int initListenFd(unsigned short port)
{
    //1.创建监听套接字
    int lfd = socket(AF_INET,SOCK_STREAM,0);
    if(lfd == -1){
        perror("socket");
        return -1;
    }
    //2.设置端口号复用 （可选）
    int opt = 1;
    int ret = setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    if(ret == -1){
        perror("setsockport");
        return -1;
    }
    //3.绑定
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port); // 主机字节序转为网络字节序
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(lfd,(struct sockaddr*)&addr,sizeof(addr));
    if(ret == -1){
        perror("bind");
        return -1;
    }
    //4. 设置监听
    ret = listen(lfd,128);
    if(ret == -1){
        perror("listen");
        return -1;
    }
    return lfd;
}

int epollRun(int lfd)
{
    //1.创建epoll实例
    int epfd = epoll_create(1);
    if(epfd == -1){
        perror("epoll_create");
        return -1;
    }
    // 2.添加监听fd lfd上树 对于监听的描述符来说只需要看一下有没有新的客户端连接
    struct epoll_event ev;
    ev.data.fd = lfd;
    ev.events = EPOLLIN; // 委托epoll(内核)帮我们检测lfd的读事件
    int ret = epoll_ctl(epfd,EPOLL_CTL_ADD,lfd,&ev);
    if(ret == -1){
        perror("epoll_ctl");
        return -1;
    }
    // 3.检测
    struct epoll_event evs[1024];
    int size = sizeof(evs) / sizeof(evs[0]);
    while (1)
    {
        int num = epoll_wait(epfd,evs,size,-1); //-1表示一直阻塞， 返回值是已经就绪的文件描述符的个数
        if(num == -1){
            perror("epoll_wait");
            return -1;
        }
        for(int i = 0; i < num; ++i){
            int fd = evs[i].data.fd;
            if(fd == lfd){
                //建立连接
                acceptClient(lfd,epfd);
            }else{
                //接收对端数据
                recvHttpRequest(fd,epfd);
            }
        }
    }
    return 0;   
}

// 接受新连接，把得到的新的文件描述符cfd也添加到epoll树上
int acceptClient(int lfd,int epfd)
{
    // 1. 创建套接字，；建立连接
    int cfd = accept(lfd,NULL,NULL); //后两个参数设置为NULL表示不关注客户端IP地址以及端口号信息
    if(cfd == -1){
        perror("accept");
        return -1;
    }
    // 2. 添加到epoll中
    int flag = fcntl(cfd,F_GETFL); //设置为非阻塞模式，主要针对于读缓存区数据循环接收
    flag |= O_NONBLOCK; //非阻塞模式
    fcntl(cfd,F_SETFL,flag);
    // 3. 将cfd添加到epoll中
    struct epoll_event ev;
    ev.data.fd = cfd;
    ev.events = EPOLLIN | EPOLLET; //设置为边沿触发模式
    int ret = epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&ev);
    if(ret == -1){
        perror("epoll_ctl");
        return -1;
    }
    return 0;
}

int recvHttpRequest(int cfd,int epfd)
{
    // 有了存储数据的内存之后，接下来就是读数据
    // 注意：前面已经把用于通信的文件描述符的事件改成了边缘非阻塞
    // 如果是边缘模式epoll检测到文件描述符对应的读事件之后，只会给我们通知一次
    // 因此需要得到这个通知之后，
    printf("开始接收数据.......\n");
    int len = 0 , total = 0;
    char buf[4096] = {0};
    char temp[1024] = {0};
    while ((len = recv(cfd,temp,sizeof(temp),0)) > 0)//这里有个优先级别的问题，这个问题>的优先级高于=，所以>前面要整体括起来，这个问题当时找了很久
    /*
    这个问题带来的后果如下：
    缓冲区数据永远不会累积：len 不是实际读取字节数，只是布尔真假；memcpy 就算拷贝，也永远拷贝1字节，即使 recv 真正读了100字节，也只拷贝1字节，剩余的数据被丢弃
    整体导致 数据读取失败、协议解析错误、连接关闭处理不当
    客户端虽然发送数据，服务器 recv 却错误读取极少或不完全，永远收不到完整请求（只能处理首字节），请求解析 parseRequestLine 拉不到有效路径，会返回 404 或崩溃
    */
    {
        if(total + len < sizeof(buf))
            memcpy(buf+total,temp,len); //按顺序写入，防止覆盖
        total += len;
    }
    //判断数据是否接收完毕
    if(len == -1 && errno == EAGAIN){
        // 说明服务器已经把客户端发过来的请求数据接收完毕了
        // 解析请求行
        char* pt = strstr(buf,"\r\n");
        int reqLen = pt - buf;
        buf[reqLen] = '\0';
        parseRequestLine(buf,cfd);
    }
    else if(len == 0){
        //说明客户端断开连接
        int ret = epoll_ctl(epfd,EPOLL_CTL_DEL,cfd,NULL);
        if(ret == -1){
            perror("epoll_ctl");
            return -1;
        }
        close(cfd);
    }else{
        perror("recv");
    }
    return 0;
}

/*
http协议知识补充：
    不管是http请求还是响应，都有若干行，每一行都是/r/n结束
*/

int parseRequestLine(const char* line,int cfd)
{
    // 解析请求行 
    // 请求行格式：GET /index.html HTT   -------------------P/1.1
    // 解析出请求方法、请求路径、协议版本
    // 请求方法：GET
    // 请求路径：/index.html
    // 协议版本：HTTP/1.1
    // 请求方法GET、请求路径/index.html、协议版本HTTP/1.1
    // 请求行长度：GET /index.html HTTP/1.1
    // 请求行长度：29
    char method[12];
    char path[1024];
    sscanf(line,"%[^ ] %[^ ]",method,path);
    printf("method: %s, path: %s\n",method,path);
    if(strcasecmp(method,"get") != 0){
        return -1;
    }
    decodeMsg(path,path);
    //处理客户端请求的静态资源（目录或者文件）
    char* file = NULL;
    if(strcmp(path,"/") == 0){
        file = "./";
    }else{
        file = path + 1;
    }
    //获取文件属性
    struct stat st;
    int ret = stat(file,&st);
    if(ret == -1){
        //文件不存在，回复404
        sendHeadMsg(cfd,404,"Not Found",getFileType(".html"),-1);
        sendFile("404.html",cfd);
        return 0;
    }
    //判断文件类型
    if(S_ISDIR(st.st_mode)){
        // 把这个目录中的内容发送给客户端
        sendHeadMsg(cfd,200,"OK",getFileType(".html"),-1);
        sendDir(file,cfd);
    }
    else {
        // 把文件的内容发送给客户端
        sendHeadMsg(cfd,200,"OK",getFileType(file),st.st_size);
        sendFile(file,cfd);
    }
    return 0;
}

int sendHeadMsg(int cfd,int status,const char* descr,const char* type,int length)
{
    //状态行
    char buf[4096] = {0};
    sprintf(buf,"http/1.1 %d %s\r\n",status,descr);
    //响应头
    sprintf(buf + strlen(buf),"content-type: %s\r\n",type);
    sprintf(buf+strlen(buf),"content-length: %d\r\n\r\n",length);
    send(cfd,buf,strlen(buf),0);
    return 0;
}

const char* getFileType(const char* name) {
    // a.jpg a.mp4 a.html
    // 自右向左查找 '.' 字符，如不存在返回NULL
    const char* dot = strrchr(name,'.');
    if(dot == NULL) 
        return "text/plain; charset=utf-8";//纯文本
    if(strcmp(dot,".html") == 0 || strcmp(dot,".htm") == 0) 
        return "text/html; charset=utf-8";
    if(strcmp(dot,".jpg")==0 || strcmp(dot,".jpeg")==0) 
        return "image/jpeg";
    if(strcmp(dot,".gif")==0)
        return "image/gif";
    if(strcmp(dot,".png")==0)
        return "image/png";
    if(strcmp(dot,".css")==0) 
        return "text/css";
    if(strcmp(dot,".au")==0)
        return "audio/basic";
    if(strcmp(dot,".wav")==0)
        return "audio/wav";
    if(strcmp(dot,".avi")==0)
        return "video/x-msvideo";
    if(strcmp(dot,".mov")==0 || strcmp(dot,".qt")==0)
        return "video/quicktime";
    if(strcmp(dot,".mpeg")==0 || strcmp(dot,".mpe")==0)
        return "video/mpeg";
    if(strcmp(dot,".vrml")==0 || strcmp(dot,".wrl")==0)
        return "model/vrml";
    if(strcmp(dot,".midi")==0 || strcmp(dot,".mid")==0)
        return "audio/midi";
    if(strcmp(dot,".mp3")==0)
        return "audio/mpeg";
    if(strcmp(dot,".ogg") == 0) 
        return "application/ogg";
    if(strcmp(dot,".pac") == 0)
        return "application/x-ns-proxy-autoconfig";
    return "text/plain; charset=utf-8";//纯文本
}

int sendFile(const char* fileName,int cfd)
{
    //打开文件
    int fd = open(fileName,O_RDONLY);
    // assert(fd > 0);
    if(fd == -1){
        perror("open");
        return -1;
    }
#if 0
    while(1)
    {
        char buf[1024];
        int len = read(fd,buf,sizeof(buf));
        if(len > 0){
            send(cfd,buf,len,0);
            usleep(10);
        }
        else if(len == 0){
            break;
        }else{
            perror("read");
        }
    }
#else
    // 把文件内容发送给客户端
    // int size = lseek(fd,0,SEEK_END);// 文件指针移动到了尾部
    // lseek(fd,0,SEEK_SET);
    // int ret = sendfile(cfd,fd,NULL,size);
    // off_t offset = 0;
    // while (offset < size){
    //     int ret = sendfile(cfd,fd,&offset,size- offset);
    //     printf("ret value: %d\n",ret);
    //     if (ret == -1 && errno == EAGAIN)
    //     {
    //         printf("没数据...\n");
    //     }
    // }
    struct stat st;
    fstat(fd,&st);
    off_t offset = 0;
    while (offset < st.st_size){
        int ret = sendfile(cfd,fd,&offset,st.st_size- offset);
        printf("ret value: %d\n",ret);
        if (ret == -1 && errno == EAGAIN)
        {
            printf("没数据...\n");
        }
    }
#endif
    close(fd);
    return 0;
}

int sendDir(const char* dirName,int cfd)
{
    char buf[4096] = {0};
    sprintf(buf,"<html><head><title>%s</title></head><body><table>",dirName);
    struct dirent** nameList;
    int num = scandir(dirName,&nameList,NULL,alphasort);
    for (int i = 0; i < num; i++)
    {
        // 取出文件名 nameList 指向的是一个指针数组 struct dirent* tmp[]
        char* name = nameList[i]->d_name;
        struct stat st;
        char subPath[1024] = {0};
        sprintf(subPath,"%s/%s",dirName,name);
        stat(subPath,&st);
        if(S_ISDIR(st.st_mode)){
            //从当前目录跳到子目录里边， /
            sprintf(buf+strlen(buf),
                    "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
                    name,name,st.st_size);
        }else{
            sprintf(buf+strlen(buf),
                    "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                    name,name,st.st_size);
        }
        send(cfd,buf,strlen(buf),0);
        memset(buf,0,sizeof(buf));
        free(nameList[i]);
        //终端会出现：已放弃，核心转存
        //注意这里两次free不是同一个东西，第一次就看成了同一个，以为是内存重复释放了，如果真写成了一个，应该会导致内存泄漏
        //导致的问题主要有：错误释放内存导致内存泄漏；重复释放内存导致程序崩溃；释放后访问无效内存导致程序崩溃
    }
    sprintf(buf,"</table></body></html>");
    send(cfd,buf,strlen(buf),0);
    free(nameList);
    return 0;  
}
/*
<html>
    <head>
        <title>test</title>
    </head>
    <body>
        <table>
            <tr>
                <td></td>
                <td></td>
            </tr>
            <tr>
                <td></td>
                <td></td>
            </tr>
        </table>
    </body>
</html>
*/

// 将字符转换为整型数
int hexToDec(char c){
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;
}
 
// 解码
// to 存储解码之后的数据, 传出参数, from被解码的数据, 传入参数
void decodeMsg(char* to,char* from) {
    for(;*from!='\0';++to,++from) {
        // isxdigit -> 判断字符是不是16进制格式, 取值在 0-f
        // Linux%E5%86%85%E6%A0%B8.jpg
        if(*from == '%' && isxdigit(from[1]) && isxdigit(from[2])){
            // 将16进制的数 -> 十进制 将这个数值赋值给了字符 int -> char
            // B2 == 178
            // 将3个字符, 变成了一个字符, 这个字符就是原始数据
            // *to = (hexToDec(from[1]) * 16) + hexToDec(from[2]);
            *to = (hexToDec(from[1]) << 4) + hexToDec(from[2]);
 
            // 跳过 from[1] 和 from[2] ，因此在当前循环中已经处理过了
            from += 2;
        }else{
            // 字符拷贝，赋值
            *to = *from;
        }
    }
    *to = '\0';
}
