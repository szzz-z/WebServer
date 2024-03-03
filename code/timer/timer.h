
#ifndef TIMER_H
#define TIMER_H

#include <arpa/inet.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <ctime>
#include <functional>
#include <queue>
#include <unordered_map>
#include <utility>

using TimeoutCallBack = std::function<void()>;
using Clock = std::chrono::high_resolution_clock;
using MS = std::chrono::milliseconds;
using TimeStamp = Clock::time_point;

class TimerNode {
 public:
  TimerNode(int id, std::chrono::time_point<std::chrono::system_clock> expires,
            TimeoutCallBack cb)
      : id_(id), expires_(expires), cb_(std::move(cb)) {}
  int id_;             // 用来标记定时器
  TimeStamp expires_;  // 设置过期时间
  TimeoutCallBack
      cb_;  // 设置一个回调函数用来方便删除定时器时将对应的HTTP连接关闭
  auto operator<(const TimerNode &t) -> bool { return expires_ < t.expires_; }
};

class HeapTimer {
 public:
  HeapTimer() { heap_.reserve(64); }

  ~HeapTimer() { Clear(); }
  // 调整指定id的定时器的到期时间。这可能需要根据新的到期时间向上或向下调整堆
  void Adjust(int id, int newExpires);

  // 向堆中添加一个新的定时器节点，或者如果节点已存在，则更新其到期时间和回调函数。
  // 新节点被添加到堆的末尾，然后使用HeapifyUp进行调整。如果节点已存在，则可能需要向上或向下调整。
  void Add(int id, int timeOut, const TimeoutCallBack &cb);

  // 执行并删除指定id的定时器节点。这涉及到查找节点、执行回调函数，然后删除节点。
  void DoWork(int id);
  // 清空整个定时器堆和id到位置的映射表ref_。
  void Clear();
  // 检查并执行所有已到期的定时器任务。它循环检查堆顶的定时器，如果已到期，则执行其回调函数并从堆中删除。
  void Tick();
  // 删除堆顶的定时器节点，通常在定时器到期并执行回调后调用。
  void Pop();
  // 返回距离下一个定时任务到期还剩余的时间（毫秒）。如果堆为空，返回-1。
  auto GetNextTick() -> int;

 private:
  void Del(size_t i);

  void HeapifyUp(size_t i);

  auto HeapifyDown(size_t idx, size_t n) -> bool;

  void SwapNode(size_t i, size_t j);

  std::vector<TimerNode> heap_;
  // 映射一个fd对应的定时器在heap_中的位置
  std::unordered_map<int, size_t> ref_;
};

#endif  // TIMER_H