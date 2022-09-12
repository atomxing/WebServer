## WebServer
用C++实现的高性能WEB服务器

---
### 功能
1. 利用IO复用技术epoll与线程池实现多线程的Reactor高并发模型；
2. 利用正则匹配与状态机解析HTTP请求报文，实现处理静态资源的请求；
3. 基于小根堆实现的定时器，关闭超时的非活动连接，降低处理器消耗；
4. 利用单例模式与阻塞队列实现异步的高性能日志系统，记录服务器运行状态；
5. 利用RAII机制实现数据库连接池，减少数据库连接建立与关闭的开销，实现了用户注册登录功能。


---
### 环境要求
* Linux
* C++11
* MySql


---
### 使用
需要先配置好对应的数据库
```bash
// 建立yourdb库
create database ylxdb;

// 创建user表
USE ylxdb;
CREATE TABLE user(
    username char(100) NULL,
    password char(100) NULL
)ENGINE=InnoDB;

// 添加数据
INSERT
make
./bin/server
```


---
### 压力测试
```bash
./webbench-1.5/webbench -c 1000 -t 10 http://ip:port/
./webbench-1.5/webbench -c 5000 -t 10 http://ip:port/
./webbench-1.5/webbench -c 10000 -t 10 http://ip:port/
```
* 测试环境: Ubuntu:20.04 cpu:R5-3600X 内存:32G
* QPS 15000+
* 