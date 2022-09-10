#include "webserver.h"

using namespace std;

WebServer::WebServer(
            int port, int trigMode, int timeoutMS, bool OptLinger,
            int sqlPort, const char* sqlUser, const  char* sqlPwd,
            const char* dbName, int connPoolNum, int threadNum,
            bool openLog, int logLevel, int logQueSize):
            port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
            timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
    {
    srcDir_ = getcwd(nullptr, 256); // 获取当前的工作路径，在程序中可以通过其他函数设置
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16);    // 路径拼接，服务器资源的根路径

    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_;
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    // 初始化事件的模式
    InitEventMode_(trigMode);

    //初试化套接字
    if(!InitSocket_()) { isClose_ = true;}

    // 日志相关初始化
    if(openLog) {
        // 因为是单例的，获取实例
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose_) { LOG_ERROR("========== Server init error! =========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent_ & EPOLLET ? "ET": "LT"),
                            (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

// 设置监听的文件描述符和通信的文件描述符的模式
void WebServer::InitEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;  // 检测对方能否正常关闭
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    // EPOLLONESHOT为了保证一个socket同时只能被一个线程处理
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;  // 连接事件 加上ET模式
        break;
    case 2:
        listenEvent_ |= EPOLLET;    //监听事件 加上ET模式
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);    // 按位与操作判断模式
}

// 如果忘记了去看epoll_et.c代码 流程是一样的
void WebServer::Start() {
    int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    // while循环 调用epoll wait()不断检测有没有数据到达
    while(!isClose_) {

        if(timeoutMS_ > 0) {
            timeMS = timer_->GetNextTick();
        }
        // 获取堆顶节点超时的时间长是多久
        // 为了防止epoller_->Wait(timeMS)一直调用，指定了超时时间再调用Epoll wait
        // 减少了Wait的调用次数，也不会一直阻塞在这里

        int eventCnt = epoller_->Wait(timeMS);  // timeMS超时时间到了就检测


        for(int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);

            if(fd == listenFd_) {   // 是监听的文件描述符
                DealListen_();  // 处理监听的操作，接受客户端来接。
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {  // 连接出现错误
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);    // 关闭这个连接
            }
            else if(events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);     // 处理读操作
            }
            else if(events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);    // 处理写操作
            }
            else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void WebServer::SendError_(int fd, const char*info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

// 关闭这个连接
void WebServer::CloseConn_(HttpConn* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

// 添加客户端
void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if(timeoutMS_ > 0) {
        // 添加定时器，当timeoutMS_为0，即超时后，断开连接CloseConn_
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }

    epoller_->AddFd(fd, EPOLLIN | connEvent_);  // 把新连接进来的客户端加入到epoll，请他帮忙监听
    
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

// 处理监听
void WebServer::DealListen_() {
    struct sockaddr_in addr;    // 保存连接客户端的信息
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len); // 接收新的连接
        
        if(fd <= 0) { return; }
        
        else if(HttpConn::userCount >= MAX_FD) {    // 连接成功，但是客户端连接数量太多
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        
        AddClient_(fd, addr);   // 添加客户端

    } while(listenEvent_ & EPOLLET);    // 监听事件是不是ET模式，如果是ET模式，要把所有事情都做完
    // 因为ET模式只会通知一次，要循环把事件全取出来，把事情做完，
}

// 处理读操作
void WebServer::DealRead_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);    // 延长超时时间
    // 添加任务到线程池队列，具体的读工作交给子线程做 Reactor模式
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

void WebServer::DealWrite_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);    // 延长超时时间
    // 添加任务到线程池队列，具体的写工作交给子线程做 Reactor模式
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

// 延长超时时间
void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}

// 子线程执行OnRead_读任务
void WebServer::OnRead_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno); // 读取客户端的数据
    if(ret <= 0 && readErrno != EAGAIN) {
        CloseConn_(client);
        return;
    }
    // 处理，业务逻辑的处理
    OnProcess(client);
}

void WebServer::OnProcess(HttpConn* client) {
    if(client->process()) {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);    // 监听是否可写
    } else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

void WebServer::OnWrite_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);   // 写
    if(client->ToWriteBytes() == 0) {
        /* 传输完成 */
        if(client->IsKeepAlive()) {
            OnProcess(client);
            return;
        }
    }
    else if(ret < 0) {
        if(writeErrno == EAGAIN) {
            /* 继续传输 */
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

/* Create listenFd */
bool WebServer::InitSocket_() {
    int ret;
    struct sockaddr_in addr;
    if(port_ > 65535 || port_ < 1024) { // 判断端口号是否符合标准
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   // 主机字节序转为网络字节序
    addr.sin_port = htons(port_);   // 端口号也转换
    struct linger optLinger = { 0 };
    if(openLinger_) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    // 创建套接字，会返回一个监听的文件描述符Fd
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    // 绑定
    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    // 监听
    ret = listen(listenFd_, 6);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    // 监听 监听那个文件描述符
    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);   // 设置非阻塞
    LOG_INFO("Server port:%d", port_);
    return true;
}

// 设置文件描述符非阻塞
int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
    // 先获取原先的值，再修改设置，等同于下面的代码
    /*
    int flag = fcntlfcntl(fd, F_GETFD, 0);
    flag = flag |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, flag);
    */
}


