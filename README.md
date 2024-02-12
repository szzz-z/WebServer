# WebServer
用C++实现的高性能WEB服务器

## 环境
* Linux ubuntu22.04
* clang version 18.0.0
* C++17
* MySql 8.0.32

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