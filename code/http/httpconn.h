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

  void Init(int sockFd, const sockaddr_in &addr);

  auto Read(int *saveErrno) -> ssize_t;

  auto Write(int *saveErrno) -> ssize_t;

  void Close();

  auto GetFd() const -> int;

  auto GetPort() const -> int;

  auto GetIP() const -> const char *;

  auto GetAddr() const -> sockaddr_in;

  auto Process() -> bool;

  auto ToWriteBytes() -> int { return iov_[0].iov_len + iov_[1].iov_len; }

  auto IsKeepAlive() const -> bool { return request_.IsKeepAlive(); }

  static bool is_et;
  static const char *src_dir;
  static std::atomic<int> user_count;

 private:
  int fd_;
  struct sockaddr_in addr_;

  bool is_close_;

  int iov_cnt_;
  struct iovec iov_[2];

  Buffer read_buff_;   // 读缓冲区
  Buffer write_buff_;  // 写缓冲区

  HttpRequest request_;
  HttpResponse response_;
};

#endif  // HTTP_CONN_H