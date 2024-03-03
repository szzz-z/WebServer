#include "httpresponse.h"

const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    {".html", "text/html"},
    {".xml", "text/xml"},
    {".xhtml", "application/xhtml+xml"},
    {".txt", "text/plain"},
    {".rtf", "application/rtf"},
    {".pdf", "application/pdf"},
    {".word", "application/nsword"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".au", "audio/basic"},
    {".mpeg", "video/mpeg"},
    {".mpg", "video/mpeg"},
    {".avi", "video/x-msvideo"},
    {".gz", "application/x-gzip"},
    {".tar", "application/x-tar"},
    {".css", "text/css "},
    {".js", "text/javascript "},
};

const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    {200, "OK"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
};

const std::unordered_map<int, std::string> HttpResponse::CODE_PATH = {
    {400, "/400.html"},
    {403, "/403.html"},
    {404, "/404.html"},
};

HttpResponse::HttpResponse() {
  code_ = -1;
  path_ = src_dir_ = "";
  is_keep_alive_ = false;
  mm_file_ = nullptr;
  mm_file_stat_ = {0};
};

HttpResponse::~HttpResponse() { UnmapFile(); }

void HttpResponse::Init(const std::string &srcDir, std::string &path,
                        bool isKeepAlive, int code) {
  assert(!srcDir.empty());
  if (mm_file_ != nullptr) {
    UnmapFile();
  }
  code_ = code;
  is_keep_alive_ = isKeepAlive;
  path_ = path;
  src_dir_ = srcDir;
  mm_file_ = nullptr;
  mm_file_stat_ = {0};
}

void HttpResponse::MakeResponse(Buffer &buff) {
  // extern int stat(const char *__restrict __file, struct stat *__restrict
  // __buf) noexcept(true) 用于获取文件或文件系统状态信息的系统调用
  // 这个函数接受两个参数：一个是文件路径名
  // const char *__restrict
  // __file:表示要查询状态的文件的路径名。通常这个参数是一个字符串，指向要获取状态信息的文件路径
  // 另一个是用于存储状态信息的结构体指针
  // struct stat *__restrict __buf: 是一个指向 struct stat
  // 类型的指针，用于存储获取到的文件状态信息
  // 函数会把文件或文件系统的相关信息填充到传入的结构体中。
  // 调用成功，返回值是 0，否则返回 -1

  /* 判断请求的资源文件 */
  if (stat((src_dir_ + path_).data(), &mm_file_stat_) < 0 ||
      S_ISDIR(mm_file_stat_.st_mode)) {
    code_ = 404;  // 如果 stat 函数执行失败（返回值小于 0）或者文件是一个目录
  } else if ((mm_file_stat_.st_mode & S_IROTH) == 0U) {
    // S_IROTH 表示其他用户（非文件所有者和文件所在组）的读取权限。
    code_ = 403;  // 文件存在但不可读
  } else if (code_ == -1) {
    code_ = 200;  // 在调用 MakeResponse 之前没有设置响应码 默认200
  }
  ErrorHtml();
  AddStateLine(buff);
  AddHeader(buff);
  AddContent(buff);
}

auto HttpResponse::File() -> char * { return mm_file_; }

auto HttpResponse::FileLen() const -> size_t { return mm_file_stat_.st_size; }

void HttpResponse::ErrorHtml() {
  if (CODE_PATH.count(code_) == 1) {
    path_ = CODE_PATH.find(code_)->second;
    stat((src_dir_ + path_).data(), &mm_file_stat_);
  }
}

void HttpResponse::AddStateLine(Buffer &buff) {
  std::string status;
  if (CODE_STATUS.count(code_) == 1) {
    status = CODE_STATUS.find(code_)->second;
  } else {
    code_ = 400;
    status = CODE_STATUS.find(400)->second;
  }
  buff.Append("HTTP/1.1 " + std::to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::AddHeader(Buffer &buff) {
  buff.Append("Connection: ");
  if (is_keep_alive_) {
    buff.Append("keep-alive\r\n");
    buff.Append("keep-alive: max=6, timeout=120\r\n");
    // max=6表示该连接最多可以承载6个HTTP请求。一旦达到这个请求上限，连接就会被关闭。这是为了避免连接过长时间不释放而占用服务器资源。
    // timeout=120
    // 表示连接的最大空闲时间为120秒。如果在这段时间内没有新的请求到达，连接就会被服务器主动关闭，以释放资源。

  } else {
    buff.Append("close\r\n");
  }
  buff.Append("Content-type: " + GetFileType() + "\r\n");
}

void HttpResponse::AddContent(Buffer &buff) {
  int src_fd = open((src_dir_ + path_).data(), O_RDONLY);  // readonly
  if (src_fd < 0) {
    ErrorContent(buff, "File NotFound!");
    return;
  }

  /* 将文件映射到内存提高文件的访问速度
      MAP_PRIVATE 建立一个写入时拷贝的私有映射*/
  // LOG_DEBUG("file path %s", (src_dir_ + path_).data());
  int *mm_ret = static_cast<int *>(
      mmap(nullptr, mm_file_stat_.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0));
  // 返回映射区域的起始地址  失败则返回MAP_FAILED “-1”
  // addr 设置为nullptr 由系统自动选择合适的地址
  // prot 映射区域的保护模式 这里表示可读
  // flags 指定映射的类型和特性
  // 这里表示私有映射，映射的对象只能被当前进程访问，对映射区域的修改不会反映到底层对象上，而是在进程内部进行的私有拷贝。
  if (*mm_ret == -1) {
    ErrorContent(buff, "File NotFound!");
    return;
  }
  mm_file_ = reinterpret_cast<char *>(mm_ret);
  close(src_fd);
  buff.Append("Content-length: " + std::to_string(mm_file_stat_.st_size) +
              "\r\n\r\n");
}

void HttpResponse::UnmapFile() {
  if (mm_file_ != nullptr) {
    munmap(mm_file_, mm_file_stat_.st_size);
    mm_file_ = nullptr;
  }
}

auto HttpResponse::GetFileType() -> std::string {
  /* 判断文件类型 */
  std::string::size_type idx = path_.find_last_of('.');
  if (idx == std::string::npos) {
    return "text/plain";
  }
  std::string suffix = path_.substr(idx);
  if (SUFFIX_TYPE.count(suffix) == 1) {
    return SUFFIX_TYPE.find(suffix)->second;
  }
  return "text/plain";  // 没找到则返回纯文本
}

void HttpResponse::ErrorContent(Buffer &buff, const std::string &message) {
  std::string body;
  std::string status;
  body += "<html><title>Error</title>";
  body += "<body bgcolor=\"ffffff\">";
  if (CODE_STATUS.count(code_) == 1) {
    status = CODE_STATUS.find(code_)->second;
  } else {
    status = "Bad Request";
  }
  body += std::to_string(code_) + " : " + status + "\n";
  body += "<p>" + message + "</p>";
  body += "<hr><em>WebServer</em></body></html>";

  buff.Append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
  buff.Append(body);
}
