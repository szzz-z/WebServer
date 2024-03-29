
#include "timer.h"

void HeapTimer::HeapifyUp(size_t i) {
  assert(i >= 0 && i < heap_.size());
  size_t j = (i - 1) / 2;
  while (j >= 0) {
    if (heap_[j] < heap_[i]) {
      break;
    }
    SwapNode(i, j);
    i = j;
    j = (i - 1) / 2;
  }
}

void HeapTimer::SwapNode(size_t i, size_t j) {
  assert(i >= 0 && i < heap_.size());
  assert(j >= 0 && j < heap_.size());
  std::swap(heap_[i], heap_[j]);
  ref_[heap_[i].id_] = i;
  ref_[heap_[j].id_] = j;
}

auto HeapTimer::HeapifyDown(size_t idx, size_t n) -> bool {
  assert(idx >= 0 && idx < heap_.size());
  assert(n >= 0 && n <= heap_.size());  // 边界[a,b)
  size_t i = idx;
  size_t j = i * 2 + 1;
  while (j < n) {
    if (j + 1 < n && heap_[j + 1] < heap_[j]) {
      j++;
    }
    if (heap_[i] < heap_[j]) {
      break;
    }
    SwapNode(i, j);
    i = j;
    j = i * 2 + 1;
  }
  return i > idx;
}

void HeapTimer::Add(int id, int timeout, const TimeoutCallBack &cb) {
  assert(id >= 0);
  size_t i;
  if (ref_.count(id) == 0) {
    /* 新节点：堆尾插入，调整堆 */
    i = heap_.size();
    ref_[id] = i;
    heap_.emplace_back(id, Clock::now() + MS(timeout), cb);
    HeapifyUp(i);
  } else {
    /* 已有结点：调整堆 */
    i = ref_[id];
    heap_[i].expires_ = Clock::now() + MS(timeout);
    heap_[i].cb_ = cb;
    if (!HeapifyDown(i, heap_.size())) {
      HeapifyUp(i);
    }
  }
}

void HeapTimer::DoWork(int id) {
  /* 删除指定id结点，并触发回调函数 */
  if (heap_.empty() || ref_.count(id) == 0) {
    return;
  }
  size_t i = ref_[id];
  TimerNode node = heap_[i];
  node.cb_();
  Del(i);
}

void HeapTimer::Del(size_t idx) {
  /* 删除指定位置的结点 */
  assert(!heap_.empty() && idx >= 0 && idx < heap_.size());
  /* 将要删除的结点换到堆尾，然后调整堆 */
  size_t i = idx;
  size_t n = heap_.size() - 1;
  assert(i <= n);
  if (i < n) {
    SwapNode(i, n);
    if (!HeapifyDown(i, n)) {
      HeapifyUp(i);
    }
  }
  /* 堆尾元素删除 */
  ref_.erase(heap_.back().id_);
  heap_.pop_back();
}

void HeapTimer::Adjust(int id, int newExpires) {
  /* 调整指定id的结点 */
  assert(!heap_.empty() && ref_.count(id) > 0);
  heap_[ref_[id]].expires_ = Clock::now() + MS(newExpires);
  if (!HeapifyDown(ref_[id], heap_.size())) {
    HeapifyUp(ref_[id]);
  }
}

void HeapTimer::Tick() {
  /* 清除超时结点 */
  if (heap_.empty()) {
    return;
  }
  while (!heap_.empty()) {
    TimerNode node = heap_.front();
    if (std::chrono::duration_cast<MS>(node.expires_ - Clock::now()).count() > 0) {
      break;
    }
    node.cb_();
    Pop();
  }
}

void HeapTimer::Pop() {
  assert(!heap_.empty());
  Del(0);
}

void HeapTimer::Clear() {
  ref_.clear();
  heap_.clear();
}

auto HeapTimer::GetNextTick() -> int {
  Tick();
  size_t res = -1;
  if (!heap_.empty()) {
    res = std::chrono::duration_cast<MS>(heap_.front().expires_ - Clock::now()).count();
    if (res < 0) {
      res = 0;
    }
  }
  return res;
}