#ifndef BUFFER_H
#define BUFFER_H

#include <sys/uio.h>
#include <unistd.h>

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

class Buffer {
 public:
  explicit Buffer(int initBuffSize = 1024);
  ~Buffer() = default;
  // 缓存区中可以写入的字节数
  auto WritableBytes() const -> size_t;
  // 缓存区中可以读取的字节数
  auto ReadableBytes() const -> size_t;
  // 返回缓冲区前部（即读指针之前的部分）的空间大小，通常用于回退读指针。
  auto PrependableBytes() const -> size_t;
  // 返回一个指向当前读指针位置的常量指针，可以用来查看或读取缓冲区内容，但不移动读指针。
  auto Peek() const -> const char *;
  // 保证将数据写入缓冲区
  void EnsureWriteable(size_t len);
  // 更新写指针位置，用于表示已经向缓冲区成功写入了len字节的数据。
  void HasWritten(size_t len);
  // 移动读指针，用于表示已经处理了len字节的数据。
  void Retrieve(size_t len);
  // 移动读指针直到end指定的位置，用于处理直到某个指定结束点的数据。
  void RetrieveUntil(const char *end);
  // 清空缓冲区，将读写指针都重置为0，并将缓冲区内容清零。
  void RetrieveAll();
  // 将缓冲区中可读的数据转换为std::string，然后清空缓冲区。
  auto RetrieveAllToStr() -> std::string;

  // 返回一个指向当前写指针位置的指针，用于开始向缓冲区写入数据。
  auto BeginWriteConst() const -> const char *;
  auto BeginWrite() -> char *;

  // 将数据写入到缓冲区
  // 向缓冲区追加数据，支持多种数据类型和来源。如果空间不足，会自动调整缓冲区大小以适应新增数据。
  void Append(const std::string &str);
  void Append(const char *str, size_t len);
  void Append(const void *data, size_t len);
  void Append(const Buffer &buff);

  // IO操作的读与写接口
  // 从文件描述符fd读取数据直接存入缓冲区，利用readv系统调用优化读取性能。如果读取的数据超出当前缓冲区可写空间，会自动扩展缓冲区并存储额外数据。
  auto ReadFd(int fd, int *Errno) -> ssize_t;
  // 将缓冲区中的数据写入到文件描述符fd，更新读指针表示数据已经被发送。
  auto WriteFd(int fd, int *Errno) -> ssize_t;

 private:
  // 返回指向缓冲区初始位置的指针
  auto BeginPtr() -> char *;
  auto BeginPtr() const -> const char *;
  // 在需要写入更多数据时，如果当前可写空间不足，该方法用于扩展缓冲区或通过移动数据来释放足够的空间。
  void AllocateSpace(size_t len);

  std::vector<char> buffer_;            // buffer的实体
  std::atomic<std::size_t> read_pos_;   // 用于指示读指针
  std::atomic<std::size_t> write_pos_;  // 用于指示写指针
};
#endif  // BUFFER_H