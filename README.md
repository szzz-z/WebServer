# WebServer
用C++实现的高性能WEB服务器

## 环境
* Linux ubuntu22.04
* clang version 19.0.0
* C++17
* MySql 8.0.32

## 功能

- 利用IO复用技术Epoll与线程池实现多线程的Reactor高并发模型；
- 利用正则与状态机解析HTTP请求报文，实现处理静态资源的请求；
- 利用标准库容器封装char，实现自动增长的缓冲区；
- 基于小根堆结构实现的定时器，关闭超时的非活动连接；
- 利用RAII机制实现了数据库连接池，减少数据库连接建立与关闭的开销，同时实现了用户注册登录功能。

## 项目启动
需要先配置好对应的数据库
```bash
// 建立webserver库
create database webserver;

// 创建user表
USE webserver;
CREATE TABLE user(
    username char(50) NULL,
    password char(50) NULL
)ENGINE=InnoDB;

// 添加数据
INSERT INTO user(username, password) VALUES('your_name', 'your_password');
```
```bash
make
./bin/server
```
## 压力测试
webbench -c X -t Y http://127.0.0.1:9006/


## 致谢
Linux高性能服务器编程，游双著.

[@qinguoyi](https://github.com/qinguoyi/TinyWebServer)