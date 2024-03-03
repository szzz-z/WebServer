#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <arpa/inet.h>
#include <fcntl.h>  // fcntl()
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>  // close()

#include <cassert>
#include <cerrno>
#include <unordered_map>

#include "../http/httpconn.h"
#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../timer/timer.h"
#include "epoller.h"

class WebServer {
 public:
  WebServer(int port, int trigMode, int timeoutMS, bool OptLinger, int sqlPort,
            const char *sqlUser, const char *sqlPwd, const char *dbName,
            int connPoolNum, int threadNum, bool openLog, int logLevel,
            int logQueSize);

  ~WebServer();
  // 启动服务器
  void Start();

 private:
  // 初始化套接字
  auto InitSocket() -> bool;
  // 根据触发模式初始化事件模式
  void InitEventMode(int trigMode);
  // 向服务器添加客户端连接
  void AddClient(int fd, sockaddr_in addr);
  // 处理监听套接字上的事件
  void DealListen();

  // 处理读、写事件(事件的处理被委托给了线程池中的任务)
  void DealWrite(HttpConn *client);
  void DealRead(HttpConn *client);

  // 发送错误信息给客户端
  void SendError(int fd, const char *info);
  // 更新客户端的超时时间
  void ExtentTime(HttpConn *client);
  // 关闭客户端连接
  void CloseConn(HttpConn *client);

  // 处理读、写事件(底层实现)
  void OnRead(HttpConn *client);
  void OnWrite(HttpConn *client);

  // 处理客户端请求的具体逻辑
  void OnProcess(HttpConn *client);

  // 服务器支持的最大文件描述符数量
  static const int MAX_FD = 65536;
  // 用于设置指定文件描述符为非阻塞模式
  static auto SetFdNonblock(int fd) -> int;

  // 存储服务器监听的端口号
  int port_;
  // 标识是否开启优雅关闭，即 SO_LINGER 选项
  // SO_LINGER
  // 是套接字选项中的一种，用于控制关闭连接时的行为。当一个套接字关闭时，操作系统会尝试将发送缓冲区中的数据发送给对方，然后等待一段时间以确保数据发送成功，最后再关闭连接。SO_LINGER选项允许设置套接字关闭的行为，特别是在存在未发送完的数据时。
  bool open_linger_;
  // 超时时间 MS
  int timeout_ms_;

  bool is_close_;
  // 监听套接字的文件描述符
  int listen_fd_;
  // 存储服务器资源目录的路径
  char *src_dir_;
  // 监听套接字关注的事件类型
  uint32_t listen_event_;
  // 连接套接字关注的事件类型
  uint32_t conn_event_;

  // 定时器，用于处理客户端连接的超时
  std::unique_ptr<HeapTimer> timer_;
  // 用于处理客户端请求
  std::unique_ptr<ThreadPool> threadpool_;
  // 用于监听和处理事件
  std::unique_ptr<Epoller> epoller_;
  // 存储客户端连接的容器，键为文件描述符，值为对应的 HttpConn 实例
  std::unordered_map<int, HttpConn> users_;
};

#endif  // WEBSERVER_H