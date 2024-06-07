/*************************************************************************
    > File Name: main.cpp
    > Author:Wux1aoyu
    >  
    > Created Time: Fri 17 May 2024 05:02:16 AM PDT
 ************************************************************************/

#include"sever.h"
using namespace std;
// 原则上 main 函数只是逻辑函数调用，具体的内容不会写在这里面
//代码量少
int main(int argc,char *argv[]){
    //启动服务器->epoll
    if(argc<3){
        cout<<"./a.out port path\n"<<endl;
        exit(0);
    }
    //argv[2]是path的路径 
    //将进程进入到当前的目录相当于cd
    chdir(argv[2]);
    //启动服务器 -》基于epoll ET 非阻塞
    unsigned short port=atoi(argv[1]);// ./后面的参数
    epollrun(port);
}
