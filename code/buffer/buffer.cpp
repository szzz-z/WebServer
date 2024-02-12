#include "buffer.h"
#include <cassert>
#include <cstddef>
#include <cstring>

Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), read_pos_(0), write_pos_(0){};

auto Buffer::ReadableBytes() const -> size_t { return write_pos_ - read_pos_; }

auto Buffer::WritableBytes() const -> size_t { return buffer_.size() - write_pos_; }

auto Buffer::PrependableBytes() const -> size_t { return read_pos_; }

auto Buffer::Peek() const -> const char * { return BeginPtr() + read_pos_; }

void Buffer::Retrieve(size_t len) {
  assert(len <= ReadableBytes());
  read_pos_ += len;
}

void Buffer::RetrieveUntil(const char *end) {
  assert(Peek() <= end);
  Retrieve(end - Peek());
}

void Buffer::RetrieveAll() {
  // bzero(&buffer_[0], buffer_.size());
  buffer_.assign(buffer_.size(), 0);
  read_pos_ = 0;
  write_pos_ = 0;
}

auto Buffer::RetrieveAllToStr() -> std::string {
  std::string str(Peek(), ReadableBytes());
  RetrieveAll();
  return str;
}

auto Buffer::BeginWriteConst() const -> const char * { return BeginPtr() + write_pos_; }

auto Buffer::BeginWrite() -> char * { return BeginPtr() + write_pos_; }

void Buffer::HasWritten(size_t len) {
  assert(len <= WritableBytes());
  write_pos_ += len;
}

void Buffer::Append(const std::string &str) { Append(str.data(), str.size()); }

void Buffer::Append(const void *data, size_t len) {
  assert(data);
  Append(static_cast<const char *>(data), len);
}

void Buffer::Append(const char *str, size_t len) {
  assert(str);
  EnsureWriteable(len);
  std::copy(str, str + len, BeginWrite());
  HasWritten(len);
}

void Buffer::Append(const Buffer &buff) { Append(buff.Peek(), buff.ReadableBytes()); }

auto Buffer::ReadFd(int fd, int *saveErrno) -> ssize_t {
  char buff[65535];
  struct iovec iov[2];
  const size_t writable = WritableBytes();

  iov[0].iov_base = BeginPtr() + write_pos_;
  iov[0].iov_len = writable;
  iov[1].iov_base = buff;
  iov[1].iov_len = sizeof(buff);

  const ssize_t len = readv(fd, iov, 2);
  if (len < 0) {
    *saveErrno = errno;
  } else if (static_cast<size_t>(len) <= writable) {
    write_pos_ += len;
  } else {
    write_pos_ = buffer_.size();
    Append(buff, len - writable);
  }
  return len;
}

auto Buffer::WriteFd(int fd, int *saveErrno) -> ssize_t {
  size_t read_size = ReadableBytes();
  ssize_t len = write(fd, Peek(), read_size);
  if (len < 0) {
    *saveErrno = errno;
    return len;
  }
  read_pos_ += len;
  return len;
}

auto Buffer::BeginPtr() -> char * { return buffer_.data(); }

auto Buffer::BeginPtr() const -> const char * { return buffer_.data(); }

void Buffer::MakeSpace(size_t len) {
  if (WritableBytes() + PrependableBytes() < len) {
    buffer_.resize(write_pos_ + len + 1);
  } else {
    size_t readable = ReadableBytes();
    std::copy(BeginPtr() + read_pos_, BeginPtr() + write_pos_, BeginPtr());
    read_pos_ = 0;
    write_pos_ = read_pos_ + readable;
    assert(readable == ReadableBytes());
  }
}