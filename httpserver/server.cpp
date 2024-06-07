#include"sever.h"
//初始化监听套接字
int initlistenFd(unsigned short port)
{
    //1.创建监听的套接字
    int lfd=socket(AF_INET,SOCK_STREAM,0);
    if(lfd==-1){
        perror("socket");
        return -1;
    }

    //2. 端口复用
    //如果服务器主动断开链接，那么将会进入TIME_WAIT 状态，等待2msl，这个时间太长了，所以就设置端口复用，继续使用端口复用，使客户端用这个端口链接，但是上一个仍处于TIME_WAIT 
    int opt=1;
    int ret = setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    if(ret==-1){
        perror("ret");
    }

    //3.绑定
    //设置文件描述符的地址ip端口
    struct sockaddr_in addr;
    addr.sin_family=AF_INET;//IPV4
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=INADDR_ANY; //0地址
    
    ret=bind(lfd,(sockaddr*)&addr,sizeof(addr));
    if(ret==-1){
        perror("bind");
        return -1;
    }

    //4.设置监听
    ret=listen(lfd,128);
     if(ret==-1){
        perror("listen");
        return -1;
    }
    
    //5.返回可用的监听的套接字
    return lfd;

}

//启动epoll模型
int epollrun(unsigned short port){
    //初始化epoll模型
    int epfd=epoll_create(1000);//创建epoll树
    if(epfd==-1){
        perror("create");
        return -1;
    }

    //初始化epoll树,将监听lfd添加上树
    int lfd=initlistenFd(port);

    struct epoll_event ev;//事件结构体
    ev.events=EPOLLIN;//检查读事件
    ev.data.fd=lfd;//将lfd添加属性中
    //添加上树
    int ret=epoll_ctl(epfd,EPOLL_CTL_ADD,lfd,&ev);
    if(ret==-1){
        perror("epoll_ctl-add");
        return -1;
    }

    //检测，循环检测，边沿ET模式，epoll非阻塞
    struct epoll_event evs[1024];
    int size=sizeof(evs)/sizeof(int);
    int flag =0;
    while (1)
    {
        if(flag==1){
            break;
        }
        int num=epoll_wait(epfd,evs,size,0);//非阻塞进行
        //遍历发生可读事件的变化的数组
        for (int  i = 0; i < num; i++)
        {
            int curfd=evs[i].data.fd;//临时变量找到变化的文件描述符
            if(curfd==lfd)//如果使监听套接字发生变化，一定是客户端请求链接
            {
                //建立链接
              int ret= acceptConn(curfd,epfd);
              if(ret==-1){
                //建立链接失败直接终止程序
                flag=1;
                break;
              }

            }
            else{
                //通信//接受http请求
                recvHttprequest(curfd,epfd);
            }
            
        }
        
    }
    


    return 0;
}

//和客户端建立新连接，并且将通信文件描述符设置成非阻塞属性
int acceptConn(int lfd,int epfd){

    //建立链接
    int cfd=accept(lfd,NULL,NULL);
    if(cfd==-1){
        perror("accept");
        return -1;
    }

    //设置通信文案描述属性为非阻塞
    int flag=fcntl(cfd,F_GETFL);
    flag|=O_NONBLOCK;
    fcntl(cfd,F_SETFL,flag);

    //通信套接字添加到epoll模型上
    struct epoll_event ev;
    ev.data.fd=cfd;
    ev.events=EPOLLIN | EPOLLET;//事件为边沿属性，检查读缓冲区；
    int ret =epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&ev);
    if(ret==-1){
        perror("epoll_ctl");
        return -1;
    }

}

//和客户端断开新链接
int disconnect(int cfd,int epfd){

    //将节点从epoll模型删除
    int ret =epoll_ctl(epfd,EPOLL_CTL_DEL,cfd,NULL);//删除操作最后一个制空
    if(ret==-1){
        perror("epoll_ctl_del");
        return -1;
    }
    //关闭通信套接字
    close(cfd);
    return 0;
}
//接受客户端http的请求消息
int recvHttprequest(int cfd,int epfd){
    //因为是边沿非阻塞模型，所以要一次性循环读

    char tmp[1024];//每次读1k数据
    char buf[4096];//每次把读的数据存到这个缓冲区里面
    //循环读数据
    int len,total=0;//total 是当前的buf的数据
    //客户端申请的都是静态资源，请求的资源内容，在请求行的第二部分
    //只需将请求完整的保存下来就可以
    //不需要解析请求头的数据，因此接受到之后不储存也是没问题的

    while((len=recv(cfd,tmp,sizeof(tmp),0))>0){
        if(len+total<sizeof(buf))//说明接受的和当前的还没超过缓冲区的大小
        {
            //有空间储存数据
            memcpy(buf+total,tmp,len);//从当前的数据往后加
        }
        total+=len;//当前的缓冲区的容量；
        buf[total] = '\0';
    }

    //循环结束了，说明读完了
    //非阻塞，缓存没有数据，返回-1，返回错误号
    if(len==-1&&errno==EAGAIN){
     //将请求行从接收的数据中拿出来 (http协议中他分了很多行，我们要拿第一行)
     //找到 \r\n就可以找到第一行
    char*pt= strstr(buf,"\r\n");//找到了\r\n之前的请求行
    int reqlen=pt-buf;//\r\n   的位置-首地址的位置
    //保留请求行
    buf[reqlen]='\0';//截断了
    //此时buf里面存在的是http的请求行的内容

     //解析请求行
    parserequestline(buf,cfd);
    }
    else if(len==0){
        cout<<"客户端断开连接了....."<<endl;
        //服务器和客户端也断开,cfd，从epoll删除文件描述符
        disconnect(cfd,epfd);
    }
    else{
        perror("recv");
        return -1;
    }
    return 0;

}

//解析请求行
int parserequestline(const char *requline,int cfd){
    //请求行分为三部分
    //GET /HELLO/WORLD/HTTP/1.1

    //1.拆分请求行，有用的是前两部分
    //提交数据的方式
    //客户端向服务器请求的文件名

    //拆分用正则表达式 sscanf
    char method[5]; //POST GET 
    char path[1024]; //存储的是目录文件地址
    sscanf(requline,"%[^ ] %[^ ]",method,path);

    //2. 判断请求的方式是不是get' ，不是get 直接忽略
    if(strcasecmp(method,"get")!=0){
        cout<<"用户提交不是get请求"<<endl;
        return -1;
    }

    //3. 判断用户访问的是文件还是目录
    // /HELLO/WORLD/ ，判断是不是  用stat
    char *file=NULL;
    if(strcmp(path,"/")==0){ //就是比较是不是/
        file="./";
    }
    else{
        file=path+1; //"./" +1 就是从h开始的
        //   hello/a.txt == ./hello/a.txt 这个目录等价   加.比较麻烦，如果什么都不加，就是从根目录找了
    }

    //属性判断 是不是文件或者目录
    struct stat st;//传出参数
    int ret=stat(file,&st);
    if(ret==-1){
        //判断失败
        //无文件发送404给客户端
        sendHeadmsg(cfd,404,"not found","text/html",-1);
        sendFile(cfd,"404.html");
    }
    if(S_ISDIR(st.st_mode)){
        //如果是目录的话将目录内容发送给客户端
    }
    else{
        //如果是普通文件,发送文件,把头信息发出去
        sendHeadmsg(cfd,200,"ok","text/html",st.st_size); //这里我们默认传输html文件
        sendFile(cfd,file);

    }

    return 0;
}

//发送头信息
int sendHeadmsg(int cfd,int status,const char *descr,const char*type,int length){
    //状态行 +消息包头 +空行
    char buf[4096];
    //http/1.1 200 ok
    sprintf(buf,"http/1.1 %d %s\r\n",status,descr);
    //消息包头 ->这里只需两个键值对
    //content-type /content-length   https://tool.oschina.net/commons去这里查
     sprintf(buf + strlen(buf), "Content-Type: %s\r\n", type);
    sprintf(buf + strlen(buf), "Content-Length: %d\r\n\r\n", length);

    // 空行
    //拼接完成之后发送
    send(cfd,buf,strlen(buf),0);//非阻塞


    return 0;

}

int sendFile(int cfd,const char *file){

    //读文件，发送给客户端
    //在发送内容之前应该有状态+消息包头，+空行+文件内容
    //这四部分数据组织好之后再发送数据吗？
    //不是 为什么，因为传输层默认人是tcp的
    //面向连接的流式传输协议-》只有最后全部发送完就可以

    int fd=open(file,O_RDONLY);//只读
    while (1)
    {
        char buf[1024];
        int len=read(fd,buf,sizeof(buf));
        if(len>0){
            //发送读出的数据
            send(cfd,buf,len,0);
        }
        else if(len==0){
            //文件读完了
            break;
        }
        else{
            perror("read");
            return -1;
        }
        
    }


    return 0;
}

