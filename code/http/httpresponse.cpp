#include "httpresponse.h"

const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    {".html", "text/html"},          {".xml", "text/xml"},          {".xhtml", "application/xhtml+xml"},
    {".txt", "text/plain"},          {".rtf", "application/rtf"},   {".pdf", "application/pdf"},
    {".word", "application/nsword"}, {".png", "image/png"},         {".gif", "image/gif"},
    {".jpg", "image/jpeg"},          {".jpeg", "image/jpeg"},       {".au", "audio/basic"},
    {".mpeg", "video/mpeg"},         {".mpg", "video/mpeg"},        {".avi", "video/x-msvideo"},
    {".gz", "application/x-gzip"},   {".tar", "application/x-tar"}, {".css", "text/css "},
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

void HttpResponse::Init(const std::string &srcDir, std::string &path, bool isKeepAlive, int code) {
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
  /* 判断请求的资源文件 */
  if (stat((src_dir_ + path_).data(), &mm_file_stat_) < 0 || S_ISDIR(mm_file_stat_.st_mode)) {
    code_ = 404;
  } else if ((mm_file_stat_.st_mode & S_IROTH) == 0U) {
    code_ = 403;
  } else if (code_ == -1) {
    code_ = 200;
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
  } else {
    buff.Append("close\r\n");
  }
  buff.Append("Content-type: " + GetFileType() + "\r\n");
}

void HttpResponse::AddContent(Buffer &buff) {
  int src_fd = open((src_dir_ + path_).data(), O_RDONLY);
  if (src_fd < 0) {
    ErrorContent(buff, "File NotFound!");
    return;
  }

  /* 将文件映射到内存提高文件的访问速度
      MAP_PRIVATE 建立一个写入时拷贝的私有映射*/
  // LOG_DEBUG("file path %s", (src_dir_ + path_).data());
  int *mm_ret = static_cast<int *>(mmap(nullptr, mm_file_stat_.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0));
  if (*mm_ret == -1) {
    ErrorContent(buff, "File NotFound!");
    return;
  }
  mm_file_ = reinterpret_cast<char *>(mm_ret);
  close(src_fd);
  buff.Append("Content-length: " + std::to_string(mm_file_stat_.st_size) + "\r\n\r\n");
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
  return "text/plain";
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
  body += "<hr><em>TinyWebServer</em></body></html>";

  buff.Append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
  buff.Append(body);
}
