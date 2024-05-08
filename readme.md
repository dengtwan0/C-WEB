# C++轻量级、高性能、高并发的Web服务器
## Introduction
用C++实现的高性能WEB服务器，经过webbenchh压力测试可以实现上万的QPS

## Function
- **网络模型**：线程池+非阻塞socket+epoll的Reactor高并发模型
- **报文解析**：利用正则与状态机解析HTTP请求报文，解决粘包问题，实现处理静态资源的请求
- **缓冲区**：实现muduo网络库的Buffer类，用于缓冲请求和响应报文
- **定时器**：基于小根堆实现的定时器，关闭超时的非活动连接
- **日志**：利用单例模式与阻塞队列实现异步的日志系统，记录服务器运行状态
- **数据库连接池**：实现RAII数据库连接池，减少数据库连接建立与关闭的开销，同时实现了用户注册登录功能

## Environment
* Linux
* Modern C++
* MySql
* Vscode
* git

## 目录树
```
.
├── code           源代码
│   ├── buffer
│   ├── http
│   ├── log
│   ├── timer
│   ├── pool
│   ├── server
│   └── main.cpp
├── resources      静态资源
│   ├── images
│   ├── video
│   ├── js
│   ├── css
|   ├── index.html
|   └── ...
├── webbench-1.5   压力测试
├── build          
│   └── Makefile
├── Makefile
└── readme.md
```
## Build & Usage
```
make
./bin/server

需要先配置好对应的数据库
bash
// 建立yourdb库
create database yourdb;

// 创建user表
USE yourdb;
CREATE TABLE user(
    username char(50) NULL,
    password char(50) NULL
)ENGINE=InnoDB;

// 添加数据
INSERT INTO user(username, password) VALUES('name', 'password');
```

## Test


服务器压力测试：
```bash
cd webbench-1.5
make
webbench -c 1000 -t 30 http://ip:port/

参数：
	-c 表示客户端数
	-t 表示时间
```





