#define _GNU_SOURCE
#include "HttpRequest.h"
#include "TcpConnection.h"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h> // isxdigit
 
#define HeaderSize 12
// 初始化
struct HttpRequest* httpRequestInit() {
    struct HttpRequest* request = (struct HttpRequest*)malloc(sizeof(struct HttpRequest));
    httpRequestReset(request);
    request->reqHeaders = (struct RequestHeader*)malloc(sizeof(struct RequestHeader) * HeaderSize);
    return request;
}
 
// 重置
void httpRequestReset(struct HttpRequest* req) {
    req->curState = ParseReqLine;
    req->method = NULL;
    req->url = NULL;
    req->version = NULL;
    req->reqHeadersNum = 0;
}
 
void httpRequestResetEx(struct HttpRequest* req) {
    free(req->method);
    free(req->url);
    free(req->version);
    if(req->reqHeaders != NULL) {
        for(int i=0;i<req->reqHeadersNum;++i) {
            free(req->reqHeaders[i].key);
            free(req->reqHeaders[i].value);
        }
        free(req->reqHeaders);
    }
    httpRequestReset(req);
}
 
void httpRequestDestroy(struct HttpRequest* req) {
    if(req != NULL) {
        httpRequestResetEx(req);
        free(req);
    }
}
 
// 获取处理状态
enum HttpRequestState httpRequestState(struct HttpRequest* req) {
    return req->curState;
}
 
// 添加请求头
void httpRequestAddHeader(struct HttpRequest* req,const char* key, const char* value) {
    req->reqHeaders[req->reqHeadersNum].key = (char*)key;
    req->reqHeaders[req->reqHeadersNum].value = (char*)value;
    req->reqHeadersNum++;
}
 
// 根据key得到请求头的value
char* httpRequestGetHeader(struct HttpRequest* req,const char* key) {
    if(req!=NULL) {
        for(int i=0;i<req->reqHeadersNum;++i) {
            if(strncasecmp(req->reqHeaders[i].key,key,strlen(key)) == 0) {
                return req->reqHeaders[i].value;
            }
        }
    }
    return NULL;
}
 
// 注意:传入指针的地址(二级指针),这个函数涉及给指针分配一块内存,
// 指针在作为参数的时候会产生一个副本,而把指针的地址作为参数传入
// 不会产生副本
 
// 如果想要在一个函数里边给外部的一级指针分配一块内存,那么需要把
// 外部的一级指针的地址传递给函数.外部的一级指针的地址
// 二级指针,把二级指针传进来之后,对它进行解引用,让其指向我们申请的
// 一块堆内存,就可以实现外部的一级指针被初始化了，也就分配到了一块内存
char* splitRequestLine(const char* start,const char* end,const char* sub,char** ptr) {
    char* space = (char*)end;
    if(sub != NULL) {
        space = memmem(start,end-start,sub,strlen(sub));
        assert(space!=NULL);
    }
    int length = space - start;
    char* tmp = (char*)malloc(length+1);
    strncpy(tmp,start,length);
    tmp[length] = '\0';
    *ptr = tmp;// 对ptr进行解引用=>*ptr(一级指针),让其指向tmp指针指向的地址
    return space+1;
}

// 解析请求行
bool parseHttpRequestLine(struct HttpRequest* req,struct Buffer* readBuf) {
    // 读取请求行
    char* end = bufferFindCRLF(readBuf);
    // 保存字符串起始位置
    char* start = readBuf->data + readBuf->readPos;
    // 保存字符串结束地址
    int lineSize = end - start;
    if(lineSize>0) {
        start = splitRequestLine(start,end," ",&req->method);// 请求方式
        start = splitRequestLine(start,end," ",&req->url);// url资源
        splitRequestLine(start,end,NULL,&req->version);// 版本
#if 0
        // get /xxx/xx.txt http/1.1
        // 请求方式
        char* space = memmem(start,lineSize," ",1);
        assert(space!=NULL);
        int methodSize = space - start;
        req->method = (char*)malloc(methodSize + 1);
        strncpy(req->method,start,methodSize);
        req->method[methodSize] = '\0';
 
        // 请求静态资源
        start = space + 1;
        space = memmem(start,end-start," ",1);
        assert(space!=NULL);
        int urlSize = space - start;
        req->url = (char*)malloc(urlSize + 1);
        strncpy(req->url,start,urlSize);
        req->url[urlSize] = '\0';
 
        // http 版本
        start = space + 1;
        req->version = (char*)malloc(end-start + 1);
        strncpy(req->version,start,end-start);
        req->version[end-start] = '\0';
#endif
        // 为解析请求头做准备
        readBuf->readPos += lineSize;
        readBuf->readPos += 2;
 
        // 修改状态
        req->curState = ParseReqHeaders;
        return true;
    }
    return false;
}
 
// 该函数处理请求头中的一行
bool parseHttpRequestHeader(struct HttpRequest* req,struct Buffer* readBuf) {
    char* end = bufferFindCRLF(readBuf);
    if(end!=NULL) {
        char* start = readBuf->data + readBuf->readPos;
        int lineSize = end - start;
        // 基于： 搜索字符串
        char* middle = memmem(start,lineSize,": ",2);    
        if(middle!=NULL) {
            // 拿出键值对
            char* key = malloc(middle - start + 1);
            strncpy(key,start,middle - start);
            key[middle - start] = '\0';// 获得key
            
            char* value = malloc(end - middle - 2 + 1);// end-(middle+2) + 1 = end - middle - 2 + 1
            strncpy(value,middle+2,end - middle - 2);
            value[end - middle - 2] = '\0';// 获得value
 
            httpRequestAddHeader(req,key,value);// 添加键值对
            // 移动读数据的位置
            readBuf->readPos += lineSize;
            readBuf->readPos += 2;
        }
        else {
            // 请求头被解析完了,跳过空行
            readBuf->readPos += 2;
            // 修改解析状态
            // 本项目忽略 post 请求，按照 get 请求处理
            req->curState = ParseReqDone;
        }
        return true;
    }
    return false;
}
 
// 解析http请求协议
bool parseHttpRequest(struct HttpRequest* req,struct Buffer* readBuf,
                      struct HttpResponse* response,struct Buffer* sendBuf,int socket) {
    bool flag = true;
    while(req->curState!=ParseReqDone) {
        switch(req->curState) {
            case ParseReqLine:
                // 解析请求行
                flag = parseHttpRequestLine(req,readBuf);
                break;
            case ParseReqHeaders:
                // 解析请求头
                flag = parseHttpRequestHeader(req,readBuf);
                break;
            case ParseReqBody:
                break;
            default:
                break;
        }
        if(!flag) {
            return flag;
        }
        // 判断是否解析完毕了，如果完毕了，需要准备回复的数据
        if(req->curState==ParseReqDone) {
            // 1.根据解析出的原始数据，对客户端的请求做出处理
            processHttpRequest(req,response);
            // 2.组织响应数据并发送给客户端
            httpResponsePrepareMsg(response,sendBuf,socket);
        }
    }
    req->curState = ParseReqLine;// 状态还原，保证还能继续处理第二条及以后的请求
    return flag;
}
 
// 处理基于get的http请求 根据解析出的原始数据，对客户端的请求做出处理
bool processHttpRequest(struct HttpRequest* req,struct HttpResponse* response) {
    if(strcasecmp(req->method,"get") != 0) {
        return -1;
    }
    decodeMsg(req->url,req->url); // 解码字符串
    // 处理客户端请求的静态资源（目录或者文件）
    char* file = NULL;
    if(strcmp(req->url,"/") == 0) {
        file = "./";
    }else {
        file = req->url + 1;
    }
    // 获取文件属性
    struct stat st;
    int ret = stat(file,&st);
    if(ret == -1) {
        // 文件不存在 -- 回复404
        // sendHeadMsg(cfd,404,"Not Found",getFileType(".html"),-1);
        // sendFile("404.html",cfd);
        strcpy(response->fileName,"404.html");
        response->statusCode = NotFound;
        strcpy(response->statusMsg,"Not Found");
        // 响应头
        httpResponseAddHeader(response,"Content-Type",getFileType(".html"));
        response->sendDataFunc = sendFile;
        return 0;
    }
    strcpy(response->fileName,file);
    response->statusCode = OK;
    strcpy(response->statusMsg,"OK!");
 
    // 判断文件类型
    if(S_ISDIR(st.st_mode)) {
        // 把这个目录中的内容发送给客户端
        // sendHeadMsg(cfd,200,"OK",getFileType(".html"),-1);
        // sendDir(file,cfd);
        // 响应头
        httpResponseAddHeader(response,"Content-Type",getFileType(".html"));
        response->sendDataFunc = sendDir;
    }
    else {
        // 把文件的内容发送给客户端
        // sendHeadMsg(cfd,200,"OK",getFileType(file),st.st_size);
        // sendFile(file,cfd);
        // 响应头
        char tmp[12] = {0};
        sprintf(tmp,"%ld",st.st_size);
        httpResponseAddHeader(response,"content-type",getFileType(file));
        httpResponseAddHeader(response,"content-length",tmp);
        response->sendDataFunc = sendFile;
    }
    return 0;
}
 
 
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
 
// 解码字符串
// to 存储解码之后的数据, 传出参数, from被解码的数据, 传入参数
void decodeMsg(char* to,char* from) {
    for(;*from!='\0';++to,++from) {
        // isxdigit -> 判断字符是不是16进制格式, 取值在 0-f
        // Linux%E5%86%85%E6%A0%B8.jpg
        if(from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])){
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
    if(strcmp(dot,".pdf") == 0)
        return "application/pdf";
    return "text/plain; charset=utf-8";//纯文本
}
 
void sendFile(const char* fileName,struct Buffer* sendBuf,int cfd) {
    // 打开文件
    int fd = open(fileName,O_RDONLY);
    if(fd < 0) {
        perror("open");
        return;
    }
    // assert(fd > 0); 
#if 1
    while (1) {
        char buf[1024];
        int len = read(fd,buf,sizeof(buf));
        if(len > 0) {
            // send(cfd,buf,len,0);
            bufferAppendData(sendBuf,buf,len);
#ifndef MSG_SEND_AUTO
            bufferSendData(sendBuf,cfd);
#endif
        }
        else if(len == 0) {
            break;
        }
        else{
            close(fd);
            perror("read");
        }
    }
#else
    // 把文件内容发送给客户端
    off_t offset = 0;
    int size = lseek(fd,0,SEEK_END);// 文件指针移动到了尾部
    lseek(fd,0,SEEK_SET);// 移动到文件头部
    while (offset < size){
        int ret = sendfile(cfd,fd,&offset,size - offset);
        printf("ret value: %d\n",ret);
        if (ret == -1 && errno == EAGAIN) {
            printf("没数据...\n");
        }
    }
#endif
    close(fd);
}
 
void sendDir(const char* dirName,struct Buffer* sendBuf,int cfd) {
    char buf[4096] = {0};
    sprintf(buf,"<html><head><title>%s</title></head><body><table>",dirName);
    struct dirent** nameList;
    int num = scandir(dirName,&nameList,NULL,alphasort);
    for(int i=0;i<num;i++) {
        // 取出文件名 nameList 指向的是一个指针数组 struct dirent* tmp[]
        char* name = nameList[i]->d_name;
        struct stat st;
        char subPath[1024] = {0};
        sprintf(subPath,"%s/%s",dirName,name);
        stat(subPath,&st);
        if(S_ISDIR(st.st_mode)) {
            // 从当前目录跳到子目录里边，/
            sprintf(buf+strlen(buf),
                "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
                name,name,st.st_size);
        }else{
            sprintf(buf+strlen(buf),
                "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                name,name,st.st_size);
        }
        // send(cfd,buf,strlen(buf),0);
        bufferAppendString(sendBuf,buf);
#ifndef MSG_SEND_AUTO
        bufferSendData(sendBuf,cfd);
#endif
        memset(buf,0,sizeof(buf));
        free(nameList[i]); 
    } 
    sprintf(buf,"</table></body></html>");
    // send(cfd,buf,strlen(buf),0);
    bufferAppendString(sendBuf,buf);
#ifndef MSG_SEND_AUTO
    bufferSendData(sendBuf,cfd);
#endif
    free(nameList);
}
 