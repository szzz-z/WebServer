#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <fcntl.h>     // open
#include <sys/mman.h>  // mmap, munmap
#include <sys/stat.h>  // stat
#include <unistd.h>    // close
#include <unordered_map>

#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse {
 public:
  HttpResponse();
  ~HttpResponse();

  void Init(const std::string &srcDir, std::string &path, bool isKeepAlive = false, int code = -1);
  void MakeResponse(Buffer &buff);
  void UnmapFile();
  auto File() -> char *;
  auto FileLen() const -> size_t;
  void ErrorContent(Buffer &buff, const std::string &message);
  auto Code() const -> int { return code_; }

 private:
  void AddStateLine(Buffer &buff);
  void AddHeader(Buffer &buff);
  void AddContent(Buffer &buff);

  void ErrorHtml();
  auto GetFileType() -> std::string;

  int code_;
  bool is_keep_alive_;

  std::string path_;
  std::string src_dir_;

  char *mm_file_;
  struct stat mm_file_stat_;

  static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
  static const std::unordered_map<int, std::string> CODE_STATUS;
  static const std::unordered_map<int, std::string> CODE_PATH;
};

#endif  // HTTP_RESPONSE_H