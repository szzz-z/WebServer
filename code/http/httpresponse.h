#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <unordered_map>

#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse {
 public:
  HttpResponse();
  ~HttpResponse();

  void Init(const std::string &srcDir, std::string &path,
            bool isKeepAlive = false, int code = -1);

  // 构建HTTP响应
  // --检查文件状态，设置正确的状态码，然后分别构建状态行、响应头和响应体
  void MakeResponse(Buffer &buff);

  // 解除文件的内存映射。
  void UnmapFile();

  // 返回指向内存映射文件的指针
  auto File() -> char *;

  // 返回内存映射文件的长度
  auto FileLen() const -> size_t;

  // 构造错误响应内容
  void ErrorContent(Buffer &buff, const std::string &message);

  // 返回响应码
  auto Code() const -> int { return code_; }

 private:
  // 根据响应码code_，构造HTTP状态行并添加到响应缓冲区buff中
  void AddStateLine(Buffer &buff);

  // 添加标准的HTTP响应头，如Connection和Content-Type
  void AddHeader(Buffer &buff);

  // 将请求的文件内容添加到响应缓冲区。如果文件存在且可访问，将其映射到内存中以提高效率
  void AddContent(Buffer &buff);

  // 如果响应码对应一个错误状态（如404）则设置path_为该错误的HTML页面路径
  void ErrorHtml();

  // 根据请求的文件路径后缀名返回对应的MIME类型。
  auto GetFileType() -> std::string;

  // 状态码
  int code_;

  // 表示连接是否应保持活动状态。如果为true，
  // 表示服务器与客户端之间的连接将保持打开状态，
  // 以便客户端可以通过同一连接发送多个请求，减少建立和关闭连接的开销。
  bool is_keep_alive_;

  // 存储请求的资源路径，
  // 即处理请求时需要访问的文件或资源的路径。
  std::string path_;

  // 表示服务器上用于查找请求资源的源目录路径。
  // 所有的文件搜索都会在这个目录下进行。
  std::string src_dir_;

  // 指向通过内存映射（mmap）方式映射的文件内容的指针。
  // 内存映射文件可以提高文件访问效率，
  // 因为它允许直接在内存中访问文件内容，而不是通过读写操作。
  char *mm_file_;

  // 存储内存映射文件的状态信息，这些信息用于构建HTTP响应头
  struct stat mm_file_stat_;

  // 文件后缀名到MIME类型的映射，用于在HTTP响应中指定正确的Content-Type
  static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;

  // HTTP状态码到状态消息的映射
  static const std::unordered_map<int, std::string> CODE_STATUS;

  // 特定状态码对应的错误页面路径的映射
  static const std::unordered_map<int, std::string> CODE_PATH;
};

#endif  // HTTP_RESPONSE_H