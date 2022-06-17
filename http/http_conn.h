#ifndef HTTP_CONNTION_H
#define HTTP_CONNTION_H

#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include "../locker/locker.h"
#include <sys/uio.h>

class http_conn {
public:

    static int m_epollfd;   // 所有的socket上的事件都被注册到同一个epoll对象中
    static int m_user_count;    // 统计用户的数量
    static const int READ_BUFFER_SIZE = 2048;   // 读缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 1024;  // 写缓冲区的大小

    http_conn() {}
    ~http_conn() {}

    void process(); // 处理客户端的请求
    void init(int sockfd, const sockaddr_in & addr);    /// 初始化新接收的连接
    void close_conn();  // 关闭连接
    bool read();    // 非阻塞的读
    bool write();   // 非阻塞的写

private:
    int m_socketfd; // 该HTTP连接的socket
    sockaddr_in m_address;  // 通信的socket地址
    char m_read_buf[READ_BUFFER_SIZE]; // 读缓冲区
    int m_read_index; // 标识读缓冲区已经读入的客户端数据的最后一个字节的位置

};




#endif