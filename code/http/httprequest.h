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
    // 行解析
    REQUEST_LINE,
    // 头部解析
    HEADERS,
    // 请求体解析
    BODY,
    // 解析完成
    FINISH,
  };

  enum HttpCode {
    // 无请求
    NO_REQUEST = 0,
    // GET请求
    GET_REQUEST,
    // 请求错误
    BAD_REQUEST,
    // 资源不存在
    NO_RESOURSE,
    // 请求被禁止
    FORBIDDENT_REQUEST,
    // 文件请求
    FILE_REQUEST,
    // 内部错误
    INTERNAL_ERROR,
    // 连接关闭
    CLOSED_CONNECTION,
  };

  HttpRequest() { Init(); }
  ~HttpRequest() = default;

  void Init();

  // 解析请求
  auto Parse(Buffer &buff) -> bool;

  // 获取请求路径
  auto Path() const -> std::string;
  auto Path() -> std::string &;
  // 获取请求方法
  auto Method() const -> std::string;
  // HTTP版本
  auto Version() const -> std::string;
  // 获取POST请求参数值
  auto GetPost(const std::string &key) const -> std::string;
  auto GetPost(const char *key) const -> std::string;
  // 是否保留连接
  auto IsKeepAlive() const -> bool;

  /*
    todo
    void HttpConn::ParseFormData() {}
    void HttpConn::ParseJson() {}
    */

 private:
  // 解析请求行
  auto ParseRequestLine(const std::string &line) -> bool;
  // 解析请求头部信息
  void ParseHeader(const std::string &line);
  // 解析请求体
  void ParseBody(const std::string &line);
  // 解析请求路径
  void ParsePath();
  // 解析POST请求内容
  void ParsePost();
  // 解析 application/x-www-form-urlencoded 格式的数据，并将解析结果保存到
  // post_ 成员变量中。
  void ParseFromUrlencoded();
  // 验证用户信息，根据参数 isLogin 的值来区分是登录验证还是注册验证。
  static auto UserVerify(std::string_view name, std::string_view pwd,
                         bool isLogin) -> bool;

  ParseState state_;
  std::string method_, path_, version_, body_;
  // 保存请求头部和 POST 请求的键值对信息
  std::unordered_map<std::string, std::string> header_;
  std::unordered_map<std::string, std::string> post_;

  static const std::unordered_set<std::string> DEFAULT_HTML;
  // HTML 标签映射
  static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
  // 将十六进制字符转换为整数
  static auto ConverHex(char ch) -> int;
};

#endif  // HTTP_REQUEST_H