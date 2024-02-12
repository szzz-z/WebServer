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
  auto WritableBytes() const -> size_t;
  auto ReadableBytes() const -> size_t;
  auto PrependableBytes() const -> size_t;

  auto Peek() const -> const char *;
  void EnsureWriteable(size_t len);
  void HasWritten(size_t len);

  void Retrieve(size_t len);
  void RetrieveUntil(const char *end);

  void RetrieveAll();
  auto RetrieveAllToStr() -> std::string;

  auto BeginWriteConst() const -> const char *;
  auto BeginWrite() -> char *;

  void Append(const std::string &str);
  void Append(const char *str, size_t len);
  void Append(const void *data, size_t len);
  void Append(const Buffer &buff);

  auto ReadFd(int fd, int *Errno) -> ssize_t;
  auto WriteFd(int fd, int *Errno) -> ssize_t;

 private:
  auto BeginPtr() -> char *;
  auto BeginPtr() const -> const char *;
  void MakeSpace(size_t len);

  std::vector<char> buffer_;
  std::atomic<std::size_t> read_pos_;
  std::atomic<std::size_t> write_pos_;
};