#include "http_conn.h"

int http_conn::m_epollfd = -1;   // 所有的socket上的事件都被注册到同一个epoll对象中
int http_conn::m_user_count = 0;    // 统计用户的数量

// 设置文件描述符非阻塞
void setnonblocking(int fd) {
    int old_flag = fcntl(fd, F_GETFL);
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd,F_SETFL, new_flag);
}

// 向epoll中添加需要监听的文件描述符
void addfd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;    // EPOLLRDHUP 断开，挂起连接

    if(one_shot) {
        event.events | EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

    // 设置文件描述符非阻塞
    setnonblocking(fd);
}

// 从epoll中移除监听的文件描述符
void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 修改文件描述符,重置socket上EPOLLONESHOT事件，以确保下一次可读时，EPOLLIN事件能被触发。
void modfd(int epollfd, int fd, int ev){
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}


// 初始化连接
void http_conn::init(int sockfd, const sockaddr_in & addr) {
    m_socketfd = sockfd;
    m_address = addr;

    // 端口复用
    int reuse = 1;
    setsockopt(m_socketfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 添加到epoll对象中
    addfd(m_epollfd, m_socketfd, true);
    m_user_count++;
}

// 关闭连接
void http_conn::close_conn() {
    if(m_socketfd != -1) {
        removefd(m_epollfd, m_socketfd);
        m_socketfd = -1;
        m_user_count--; // 关闭一个连接，用户数减一
    }
}

bool http_conn::read() {
    printf("一次性读完数据\n");
    return true;
}

bool http_conn::write() {
    printf("一次性写完数据\n");
    return true;
}

// 由线程池中的工作线程调用，这是处理HTTP请求的入口函数
void http_conn::process() {
    
    // 解析HTTP请求

    printf("parse request, create respoonse\n");

    // 生成响应

}