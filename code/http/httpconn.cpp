#include "httpconn.h"

const char *HttpConn::src_dir;
std::atomic<int> HttpConn::user_count;
bool HttpConn::is_et;

HttpConn::HttpConn() {
  fd_ = -1;
  addr_ = {0};
  is_close_ = true;
};

HttpConn::~HttpConn() { Close(); };

void HttpConn::Init(int fd, const sockaddr_in &addr) {
  assert(fd > 0);
  user_count++;
  addr_ = addr;
  fd_ = fd;
  write_buff_.RetrieveAll();
  read_buff_.RetrieveAll();
  is_close_ = false;
  // LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(),
  // (int)userCount);
}

void HttpConn::Close() {
  response_.UnmapFile();
  if (!is_close_) {
    is_close_ = true;
    user_count--;
    close(fd_);
    // LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(),
    // (int)userCount);
  }
}

auto HttpConn::GetFd() const -> int { return fd_; };

auto HttpConn::GetAddr() const -> struct sockaddr_in {
  return addr_;
}

auto HttpConn::GetIP() const -> const char* {
  return inet_ntoa(addr_.sin_addr);
  // 该函数用于将一个 struct in_addr 结构体中的 IPv4
  // 地址转换为点分十进制字符串表示形式。在这里，addr_.sin_addr 是一个
  // sockaddr_in 结构体中的成员，表示客户端的 IPv4 地址
}

// struct sockaddr_in {
//     short sin_family; // 地址族，通常为 AF_INET
//     unsigned short sin_port; // 端口号
//     struct in_addr sin_addr; // IPv4 地址
//     char sin_zero[8]; // 用于填充结构体，保证和 sockaddr 结构体大小相同
// };

auto HttpConn::GetPort() const -> int { return addr_.sin_port; }

auto HttpConn::Read(int *saveErrno) -> ssize_t {
  ssize_t len = -1;
  do {
    len = read_buff_.ReadFd(fd_, saveErrno);
    if (len <= 0) {
      break;
    }
  } while (is_et);
  return len;
}

auto HttpConn::Write(int *saveErrno) -> ssize_t {
  ssize_t len = -1;
  do {
    len = writev(fd_, iov_, iov_cnt_);
    // 将 iov_ 数组中的数据写入到文件描述符 fd_ 中
    if (len <= 0) {
      *saveErrno = errno;
      break;
    }

    // 以下为更新缓冲区及IOVEC
    if (iov_[0].iov_len + iov_[1].iov_len == 0) {
      break;
    } /* 传输结束 */
    if (static_cast<size_t>(len) > iov_[0].iov_len) {
      iov_[1].iov_base =
          static_cast<uint8_t *>(iov_[1].iov_base) + (len - iov_[0].iov_len);
      iov_[1].iov_len -= (len - iov_[0].iov_len);
      if (iov_[0].iov_len != 0U) {
        write_buff_.RetrieveAll();
        iov_[0].iov_len = 0;
      }
    } else {
      iov_[0].iov_base = static_cast<uint8_t *>(iov_[0].iov_base) + len;
      iov_[0].iov_len -= len;
      write_buff_.Retrieve(len);
    }
  } while (is_et || ToWriteBytes() > 10240);
  return len;
}

auto HttpConn::Process() -> bool {
  request_.Init();
  if (read_buff_.ReadableBytes() <= 0) {
    return false;
  }
  if (request_.Parse(read_buff_)) {
    // LOG_DEBUG("%s", request_.Path().c_str());
    response_.Init(src_dir, request_.Path(), request_.IsKeepAlive(), 200);
  } else {
    response_.Init(src_dir, request_.Path(), false, 400);
  }

  response_.MakeResponse(write_buff_);
  // 状态行+响应头 在缓冲区中
  iov_[0].iov_base = const_cast<char *>(write_buff_.Peek());
  iov_[0].iov_len = write_buff_.ReadableBytes();
  iov_cnt_ = 1;

  // 映射到内存中的文件
  if (response_.FileLen() > 0 && (response_.File() != nullptr)) {
    iov_[1].iov_base = response_.File();
    iov_[1].iov_len = response_.FileLen();
    iov_cnt_ = 2;
  }
  // LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iov_cnt_,
  // ToWriteBytes());
  return true;
}
