#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <mysql/mysql.h>  //mysql
#include <cerrno>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "../pool/sqlconnpool.h"

class HttpRequest {
 public:
  enum ParseState {
    REQUEST_LINE,
    HEADERS,
    BODY,
    FINISH,
  };

  enum HttpCode {
    NO_REQUEST = 0,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURSE,
    FORBIDDENT_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION,
  };

  HttpRequest() { Init(); }
  ~HttpRequest() = default;

  void Init();
  auto Parse(Buffer &buff) -> bool;

  auto Path() const -> std::string;
  auto Path() -> std::string &;
  auto Method() const -> std::string;
  auto Version() const -> std::string;
  auto GetPost(const std::string &key) const -> std::string;
  auto GetPost(const char *key) const -> std::string;

  auto IsKeepAlive() const -> bool;

  /*
  todo
  void HttpConn::ParseFormData() {}
  void HttpConn::ParseJson() {}
  */

 private:
  auto ParseRequestLine(const std::string &line) -> bool;
  void ParseHeader(const std::string &line);
  void ParseBody(const std::string &line);

  void ParsePath();
  void ParsePost();
  void ParseFromUrlencoded();

  static auto UserVerify(const std::string &name, const std::string &pwd, bool isLogin) -> bool;

  ParseState state_;
  std::string method_, path_, version_, body_;
  std::unordered_map<std::string, std::string> header_;
  std::unordered_map<std::string, std::string> post_;

  static const std::unordered_set<std::string> DEFAULT_HTML;
  static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
  static auto ConverHex(char ch) -> int;
};

#endif  // HTTP_REQUEST_H