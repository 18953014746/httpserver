
#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <pthread.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <strings.h>
#include<dirent.h>

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

// 初始化监听的文件描述符
int initlistenFd(unsigned short port);

// 启动 epoll 模型
int epollrun(unsigned short port);

// 建立新连接
int acceptConn(int lfd, int epfd);

// 接收 HTTP 请求
int recvHttprequest(int cfd, int epfd);

// 解析请求行
int parserequestline(const char *requline, int cfd);

// 发送头信息
int sendHeadmsg(int cfd, int status, const char *descr, const char *type, int length);

//发送目录
int senddir(int cfd,char*dirname);

// 发送文件
int sendFile(int cfd, const char *file);

// 断开连接
int disconnect(int cfd, int epfd);


#ifdef __cplusplus
}
#endif

#endif // SERVER_H
