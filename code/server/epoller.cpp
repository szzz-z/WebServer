#include "epoller.h"

// 传给epoll_create的参数512用于指定内核初始分配的事件表大小，
// 但实际上这个参数在较新的Linux版本中已经被忽略，
// 因为epoll实例的大小会根据需要动态调整。
// 通过调用epoll_create函数创建一个epoll实例，
// 并将返回的文件描述符存储在epoll_fd_成员变量中。
Epoller::Epoller(int maxEvent) : epoll_fd_(epoll_create(512)), events_(maxEvent) {
  assert(epoll_fd_ >= 0 && !events_.empty());
}

Epoller::~Epoller() { close(epoll_fd_); }  // 关闭epoll_fd_指向的epoll实例。

// 向epoll实例中添加一个文件描述符fd，并指定关注的事件events。
// 使用epoll_ctl函数，操作类型为EPOLL_CTL_ADD，向epoll实例注册文件描述符及其关联的事件。
// 如果操作成功返回true，失败返回false。
auto Epoller::AddFd(int fd, uint32_t events) -> bool {
  if (fd < 0) {
    return false;
  }
  epoll_event ev = {0};
  ev.data.fd = fd;
  ev.events = events;
  return 0 == epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
}

// 修改epoll实例中已注册文件描述符的关注事件。
// 操作过程与AddFd相似，但操作类型为EPOLL_CTL_MOD。
// 成功或失败返回值与AddFd相同。
auto Epoller::ModFd(int fd, uint32_t events) -> bool {
  if (fd < 0) {
    return false;
  }
  epoll_event ev = {0};
  ev.data.fd = fd;
  ev.events = events;
  return 0 == epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
}

// 从epoll实例中删除一个文件描述符。
// 使用epoll_ctl，操作类型为EPOLL_CTL_DEL。
// 成功或失败返回值与AddFd相同。
auto Epoller::DelFd(int fd) -> bool {
  if (fd < 0) {
    return false;
  }
  epoll_event ev = {0};
  return 0 == epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ev);
}

// 调用epoll_wait等待事件的发生。
// timeoutMs参数指定超时时间，单位为毫秒。
// 如果timeoutMs为-1，则无限等待直到事件发生。
// 返回值为就绪事件的数量。
auto Epoller::Wait(int timeoutMs) -> int {
  return epoll_wait(epoll_fd_, events_.data(), static_cast<int>(events_.size()), timeoutMs);
}
// 在调用epoll_wait函数时，events参数指向的数组才会被填充与发生改变。epoll_wait函数的作用是等待epoll实例监控的文件描述符上的事件发生，
// 直到有事件发生或者达到指定的超时时间为止。在事件发生（例如某个文件描述符变得可读或可写）之前，epoll_wait会保持阻塞状态。
// 一旦有事件发生，或者是超时时间到达，epoll_wait函数就会返回。
// 它会将所有就绪的事件，也就是那些发生了的、你之前用epoll_ctl注册过感兴趣的事件，填充到你传入的events数组中。
// 每个就绪的事件都会被填充为events数组的一个元素，epoll_wait同时返回就绪事件的总数。



// 这两个函数用于访问Wait方法返回后，在events_向量中存储的事件。
// GetEventFd返回第i个事件的文件描述符。
// GetEvents返回第i个事件的类型（如EPOLLIN、EPOLLOUT等）。
auto Epoller::GetEventFd(size_t i) const -> int {
  assert(i < events_.size() && i >= 0);
  return events_[i].data.fd;
}

auto Epoller::GetEvents(size_t i) const -> uint32_t {
  assert(i < events_.size() && i >= 0);
  return events_[i].events;
}