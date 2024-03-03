#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <arpa/inet.h>  // sockaddr_in
#include <sys/types.h>
#include <sys/uio.h>  // readv/writev

#include <cerrno>
#include <cstdlib>  // atoi()

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "httprequest.h"
#include "httpresponse.h"

class HttpConn {
 public:
  HttpConn();

  ~HttpConn();
  // 初始化连接
  void Init(int sockFd, const sockaddr_in &addr);
  // 从套接字读入缓冲区
  auto Read(int *saveErrno) -> ssize_t;
  // 从缓冲区写入套接字
  auto Write(int *saveErrno) -> ssize_t;

  void Close();
  // 获取套接字文件描述符
  auto GetFd() const -> int;
  // 获取端口号
  auto GetPort() const -> int;
  // 获取客户端ip地址
  auto GetIP() const -> const char *;
  // 获取客户端地址
  auto GetAddr() const -> sockaddr_in;
  // 处理请求，生成响应
  auto Process() -> bool;
  // 返回需要写入套接字的字节数，用于检查是否需要继续写入数据
  auto ToWriteBytes() -> int { return iov_[0].iov_len + iov_[1].iov_len; }
  // 指示当前连接是否为持久连接
  auto IsKeepAlive() const -> bool { return request_.IsKeepAlive(); }
  // 边缘触发
  static bool is_et;
  // 保存服务器资源目录的路径
  static const char *src_dir;
  // 记录连接到服务器的用户数量
  static std::atomic<int> user_count;

 private:
  int fd_;
  // 保存客户端的地址信息
  struct sockaddr_in addr_;
  // 标识连接是否已关闭
  bool is_close_;

  int iov_cnt_;
  // 用于向客户端（fd_）发送数据
  struct iovec iov_[2];

  Buffer read_buff_;
  Buffer write_buff_;

  HttpRequest request_;
  HttpResponse response_;
};

#endif  // HTTP_CONN_H