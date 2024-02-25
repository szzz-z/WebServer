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
  WebServer(int port, int trigMode, int timeoutMS, bool OptLinger, int sqlPort, const char *sqlUser, const char *sqlPwd,
            const char *dbName, int connPoolNum, int threadNum, bool openLog, int logLevel, int logQueSize);

  ~WebServer();
  void Start();

 private:
  auto InitSocket() -> bool;
  void InitEventMode(int trigMode);
  void AddClient(int fd, sockaddr_in addr);

  void DealListen();
  void DealWrite(HttpConn *client);
  void DealRead(HttpConn *client);

  void SendError(int fd, const char *info);
  void ExtentTime(HttpConn *client);
  void CloseConn(HttpConn *client);

  void OnRead(HttpConn *client);
  void OnWrite(HttpConn *client);
  void OnProcess(HttpConn *client);

  static const int MAX_FD = 65536;

  static auto SetFdNonblock(int fd) -> int;

  int port_;
  bool open_linger_;
  int timeout_ms_; /* 毫秒MS */
  bool is_close_;
  int listen_fd_;
  char *src_dir_;

  uint32_t listen_event_;
  uint32_t conn_event_;

  std::unique_ptr<HeapTimer> timer_;
  std::unique_ptr<ThreadPool> threadpool_;
  std::unique_ptr<Epoller> epoller_;
  std::unordered_map<int, HttpConn> users_;
};

#endif  // WEBSERVER_H