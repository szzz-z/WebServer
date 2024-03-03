#ifndef EPOLLER_H
#define EPOLLER_H

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <cassert>
#include <cerrno>
#include <vector>

class Epoller {
 public:
  explicit Epoller(int maxEvent = 1024);

  ~Epoller();
// 向epoll实例中添加一个文件描述符fd，并指定关注的事件events。
  auto AddFd(int fd, uint32_t events) -> bool;
// 修改epoll实例中已注册文件描述符的关注事件。
  auto ModFd(int fd, uint32_t events) -> bool;

// 从epoll实例中删除一个文件描述符。
  auto DelFd(int fd) -> bool;
// 调用epoll_wait等待事件的发生。
  auto Wait(int timeoutMs = -1) -> int;

// 这两个函数用于访问Wait方法返回后，在events_(就绪队列)中存储的事件。
  auto GetEventFd(size_t i) const -> int;
  auto GetEvents(size_t i) const -> uint32_t;

 private:
  int epoll_fd_;

  std::vector<struct epoll_event> events_;  // 就绪队列
};
#endif  // EPOLLER_H