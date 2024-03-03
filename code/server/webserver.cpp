#include "webserver.h"

WebServer::WebServer(int port, int trigMode, int timeoutMS, bool OptLinger,
                     int sqlPort, const char *sqlUser, const char *sqlPwd,
                     const char *dbName, int connPoolNum, int threadNum,
                     bool openLog, int logLevel, int logQueSize)
    : port_(port),
      open_linger_(OptLinger),
      timeout_ms_(timeoutMS),
      is_close_(false),
      timer_(new HeapTimer()),
      threadpool_(new ThreadPool(threadNum)),
      epoller_(new Epoller()) {
  src_dir_ = getcwd(nullptr, 256);
  assert(src_dir_);
  strncat(src_dir_, "/resources/", 16);
  HttpConn::user_count = 0;
  HttpConn::src_dir = src_dir_;
  SqlConnPool::Instance()->Init("127.0.0.1", sqlPort, sqlUser, sqlPwd, dbName,
                                connPoolNum);
  /// 服务器中可以改成localhost 访问    但是本地只能127.0.0.1 不知道为啥
  InitEventMode(trigMode);
  if (!InitSocket()) {
    is_close_ = true;
  }

  //   if (openLog) {
  //     Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
  //     if (isClose_) {
  //       LOG_ERROR("========== Server init error!==========");
  //     } else {
  //       LOG_INFO("========== Server init ==========");
  //       LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger ? "true" :
  //       "false"); LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
  //                (listenEvent_ & EPOLLET ? "ET" : "LT"),
  //                (connEvent_ & EPOLLET ? "ET" : "LT"));
  //       LOG_INFO("LogSys level: %d", logLevel);
  //       LOG_INFO("srcDir: %s", HttpConn::srcDir);
  //       LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum,
  //                threadNum);
  //     }
  //   }
}

WebServer::~WebServer() {
  close(listen_fd_);
  is_close_ = true;
  free(src_dir_);
  SqlConnPool::Instance()->ClosePool();
}

void WebServer::InitEventMode(int trigMode) {
  // 表示监听事件的默认行为为检测到对端套接字关闭连接时触发
  listen_event_ = EPOLLRDHUP;
  // 表示连接事件的默认行为为单次触发模式，并且也会检测对端套接字关闭连接。
  conn_event_ = EPOLLONESHOT | EPOLLRDHUP;
  switch (trigMode) {
    case 0:
      break;
    case 1:  // EPOLLET表示启用边缘触发
      conn_event_ |= EPOLLET;
      break;
    case 2:
      listen_event_ |= EPOLLET;
      break;
    case 3:
    //   listen_event_ |= EPOLLET;
    //   conn_event_ |= EPOLLET;
    //   break;
    default:
      listen_event_ |= EPOLLET;
      conn_event_ |= EPOLLET;
      break;
  }
  HttpConn::is_et = ((conn_event_ & EPOLLET) != 0U);
}

void WebServer::Start() {
  int time_ms = -1;  // epoll wait timeout == -1 无事件将阻塞
  if (!is_close_) {
    // LOG_INFO("========== Server start ==========");
  }
  while (!is_close_) {
    if (timeout_ms_ > 0) {
      time_ms = timer_->GetNextTick();
    }
    int event_cnt = epoller_->Wait(time_ms);
    for (int i = 0; i < event_cnt; i++) {
      /* 处理事件 */
      int fd = epoller_->GetEventFd(i);
      uint32_t events = epoller_->GetEvents(i);
      if (fd == listen_fd_) {
        DealListen();
      } else if ((events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) != 0U) {
        // EPOLLRDHUP：表示对端套接字关闭连接或者发生了对等方关机。当远程套接字关闭连接时，此事件将被触发。
        // EPOLLHUP：表示发生了挂起事件。这可能是由于对端套接字关闭了连接或者发生了异常情况。
        // EPOLLERR：表示发生了错误事件。通常，这表明套接字发生了错误，如连接重置或其他异常情况。
        assert(users_.count(fd) > 0);
        CloseConn(&users_[fd]);
      } else if ((events & EPOLLIN) != 0U) {
        assert(users_.count(fd) > 0);
        DealRead(&users_[fd]);
      } else if ((events & EPOLLOUT) != 0U) {
        assert(users_.count(fd) > 0);
        DealWrite(&users_[fd]);
      } else {
        // LOG_ERROR("Unexpected event");
      }

      // EPOLLOUT：表示套接字可以进行写操作。当套接字的发送缓冲区变为可写时，此事件将被触发，表示可以向套接字写入数据了。
      // EPOLLIN：表示套接字可以进行读操作。当套接字的接收缓冲区中有数据可供读取时，此事件将被触发，表示可以从套接字读取数据了。
    }
  }
}

void WebServer::SendError(int fd, const char *info) {
  assert(fd > 0);
  int ret = send(fd, info, strlen(info), 0);
  if (ret < 0) {
    // LOG_WARN("send error to client[%d] error!", fd);
  }
  close(fd);
}

void WebServer::CloseConn(HttpConn *client) {
  assert(client);
  // LOG_INFO("Client[%d] quit!", client->GetFd());
  epoller_->DelFd(client->GetFd());
  client->Close();
}

void WebServer::AddClient(int fd, sockaddr_in addr) {
  assert(fd > 0);
  users_[fd].Init(fd, addr);
  if (timeout_ms_ > 0) {
    timer_->Add(fd, timeout_ms_,
                [this, capture0 = &users_[fd]] { CloseConn(capture0); });
  }  // capture0为局部变量名
  epoller_->AddFd(fd, EPOLLIN | conn_event_);
  // 默认设置为读事件(EPOLLIN)是因为在
  // Web服务器中，最常见的操作是从客户端读取请求数据，
  // 因此在客户端与服务器建立连接后，首先需要准备好读取客户端发送的数据。因此，
  // 将文件描述符添加到epoll 实例时，默认设置为监听读事件（EPOLLIN）。
  SetFdNonblock(fd);
  // LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

void WebServer::DealListen() {
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  do {
    int fd =
        accept(listen_fd_, reinterpret_cast<struct sockaddr *>(&addr), &len);
    if (fd <= 0) {
      return;
    }
    if (HttpConn::user_count >= MAX_FD) {
      SendError(fd, "Server busy!");
      // LOG_WARN("Clients is full!");
      return;
    }
    AddClient(fd, addr);
  } while ((listen_event_ & EPOLLET) != 0U);
}

void WebServer::DealRead(HttpConn *client) {
  assert(client);
  ExtentTime(client);
  threadpool_->Submit([this, client] { OnRead(client); });
}

void WebServer::DealWrite(HttpConn *client) {
  assert(client);
  ExtentTime(client);
  threadpool_->Submit([this, client] { OnWrite(client); });
}

void WebServer::ExtentTime(HttpConn *client) {
  assert(client);
  if (timeout_ms_ > 0) {
    timer_->Adjust(client->GetFd(), timeout_ms_);
  }
}

void WebServer::OnRead(HttpConn *client) {
  assert(client);
  int ret = -1;
  int read_errno = 0;
  ret = client->Read(&read_errno);
  if (ret <= 0 && read_errno != EAGAIN) {
    CloseConn(client);
    return;
  }
  OnProcess(client);
}

void WebServer::OnProcess(HttpConn *client) {
  // 调用 client->Process() 处理客户端请求。如果处理结果为
  // true，则表示请求处理完毕，且有数据要发送给客户端，因此需要将连接的 epoll
  // 事件设置为 EPOLLOUT（写事件就绪）。
  // 如果处理结果为 false，表示需要继续读取客户端的数据，因此将连接的 epoll
  // 事件设置为 EPOLLIN（读事件就绪）。
  if (client->Process()) {
    epoller_->ModFd(client->GetFd(), conn_event_ | EPOLLOUT);
  } else {
    epoller_->ModFd(client->GetFd(), conn_event_ | EPOLLIN);
  }
}

void WebServer::OnWrite(HttpConn *client) {
  assert(client);
  int ret = -1;
  int write_errno = 0;
  ret = client->Write(&write_errno);
  if (client->ToWriteBytes() == 0) {
    /* 传输完成 */
    if (client->IsKeepAlive()) {
      OnProcess(client);
      return;
    }
  } else if (ret < 0) {
    // EAGAIN表示当前的非阻塞操作无法立即完成，建议稍后再试。在 POSIX
    // 兼容的操作系统中，当进行非阻塞输入输出操作（如读取、写入等）时，如果没有足够的资源可供立即完成该操作（例如，非阻塞读取操作时没有数据可读，或非阻塞写入操作时输出缓冲区已满），操作系统不会让调用进程阻塞等待，而是返回这个错误码。
    if (write_errno == EAGAIN) {
      /* 继续传输 */
      // 当前不能写更多数据，需要等待下一次写事件就绪再继续写入。此时，将连接的
      // epoll 事件设置为 EPOLLOUT
      epoller_->ModFd(client->GetFd(), conn_event_ | EPOLLOUT);
      return;
    }  // 如果写操作失败且不是因为 EAGAIN，则关闭连接
  }
  CloseConn(client);
}

/* Create listenFd */
auto WebServer::InitSocket() -> bool {
  int ret;
  struct sockaddr_in addr;
  if (port_ > 65535 || port_ < 1024) {
    // LOG_ERROR("Port:%d error!", port_);
    return false;
  }
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port_);
  struct linger opt_linger = {0};
  if (open_linger_) {
    // 优雅关闭: 直到所剩数据发送完毕或超时
    opt_linger.l_onoff = 1;
    opt_linger.l_linger = 1;
  }
  // 调用 socket 函数创建一个面向连接的 TCP
  // 套接字（SOCK_STREAM），并检查是否创建成功。
  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    // LOG_ERROR("Create socket error!", port_);
    return false;
  }

  ret = setsockopt(listen_fd_, SOL_SOCKET, SO_LINGER, &opt_linger,
                   sizeof(opt_linger));
  if (ret < 0) {
    close(listen_fd_);
    // LOG_ERROR("Init linger error!", port_);
    return false;
  }

  int optval = 1;
  /* 端口复用 */
  /* 只有最后一个套接字会正常接收数据。 */
  ret = setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR,
                   static_cast<const void *>(&optval), sizeof(int));

  if (ret == -1) {
    // LOG_ERROR("set socket setsockopt error !");
    close(listen_fd_);
    return false;
  }
  // 将前面设置的地址信息绑定到监听套接字上。如果绑定失败，函数返回 false。
  ret = bind(listen_fd_, reinterpret_cast<struct sockaddr *>(&addr),
             sizeof(addr));
  if (ret < 0) {
    // LOG_ERROR("Bind Port:%d error!", port_);
    close(listen_fd_);
    return false;
  }
  // 通过 listen 函数使套接字进入监听状态，准备接受连接请求。listen
  // 函数的第二个参数指定了套接字的最大待处理连接队列长度。
  ret = listen(listen_fd_, 6);
  if (ret < 0) {
    // LOG_ERROR("Listen port:%d error!", port_);
    close(listen_fd_);
    return false;
  }
  // 将监听套接字添加到 epoll 事件监听中，关注的事件包括
  // EPOLLIN（表示有新的连接请求）以及其他通过 listen_event_
  // 指定的事件。如果添加失败，函数返回 false。
  ret = static_cast<int>(epoller_->AddFd(listen_fd_, listen_event_ | EPOLLIN));
  if (ret == 0) {
    // LOG_ERROR("Add listen error!");
    close(listen_fd_);
    return false;
  }
  SetFdNonblock(listen_fd_);
  // LOG_INFO("Server port:%d", port_);
  return true;
}

auto WebServer::SetFdNonblock(int fd) -> int {
  assert(fd > 0);
  // 使用 F_SETFL 命令来设置文件描述符的状态标志。fcntl 的第三个参数是通过另一个
  // fcntl 调用获取的当前文件描述符的标志（使用 F_GETFD 命令），并与 O_NONBLOCK
  // 标志位进行按位或操作。这样，无论文件描述符的当前状态如何，O_NONBLOCK
  // 标志都会被添加到其中，使文件描述符进入非阻塞模式。
  return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}
