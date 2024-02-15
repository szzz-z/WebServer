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

  auto AddFd(int fd, uint32_t events) -> bool;

  auto ModFd(int fd, uint32_t events) -> bool;

  auto DelFd(int fd) -> bool;

  auto Wait(int timeoutMs = -1) -> int;

  auto GetEventFd(size_t i) const -> int;

  auto GetEvents(size_t i) const -> uint32_t;

 private:
  int epoll_fd_;

  std::vector<struct epoll_event> events_;  // 就绪队列
};
#endif  // EPOLLER_H