#include "httprequest.h"

const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML{
    "/index", "/register", "/login", "/welcome", "/video", "/picture",
};

const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG{
    {"/register.html", 0},
    {"/login.html", 1},
};

void HttpRequest::Init() {
  method_ = path_ = version_ = body_ = "";
  state_ = REQUEST_LINE;
  header_.clear();
  post_.clear();
}

auto HttpRequest::IsKeepAlive() const -> bool {
  if (header_.count("Connection") == 1) {
    return header_.find("Connection")->second == "keep-alive" &&
           version_ == "1.1";
  }
  return false;
}

auto HttpRequest::Parse(Buffer &buff) -> bool {
  constexpr char crlf[] = "\r\n";
  if (buff.ReadableBytes() <= 0) {
    return false;
  }
  while ((buff.ReadableBytes() != 0U) && state_ != FINISH) {
    const char *line_end =
        std::search(buff.Peek(), buff.BeginWriteConst(), crlf, crlf + 2);
    std::string line(buff.Peek(), line_end);
    switch (state_) {
      case REQUEST_LINE:
        if (!ParseRequestLine(line)) {
          return false;
        }
        ParsePath();
        break;
      case HEADERS:
        ParseHeader(line);
        if (buff.ReadableBytes() <= 2) {
          state_ = FINISH;
        }
        break;
      case BODY:
        ParseBody(line);
        break;
      default:
        break;
    }
    if (line_end == buff.BeginWrite()) {
      break;
    }
    buff.RetrieveUntil(line_end + 2);  // 过滤\r\n
  }
  // LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(),
  // version_.c_str());
  return true;
}

void HttpRequest::ParsePath() {
  if (path_ == "/") {
    path_ = "/index.html";  // 默认访问
  } else {
    for (auto &item : DEFAULT_HTML) {
      if (item == path_) {
        path_ += ".html";
        break;
      }
    }
  }
}

auto HttpRequest:: ParseRequestLine(const std::string &line) -> bool {
  std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
  //^ 表示匹配行的开头；
  //([^ ]*)
  // 表示匹配零个或多个非空格字符，用于捕获请求方法、请求路径和HTTP协议版本；
  // HTTP/ 表示匹配字符串 "HTTP/"；
  //([^ ]*)$ 表示匹配零个或多个非空格字符，并且匹配行的结尾。
  // 类似GET /index.html HTTP/1.1

  // std::smatch 对象可以存储多个匹配结果，并且提供了一些方法来访问这些匹配结果
  // 使用 sub_match 对象的 operator[] 或 at() 方法来访问匹配结果的特定部分。
  // 由(  )分隔子结果
  // 分组是按照括号出现的顺序编号的，从1开始（整个正则表达式匹配的文本作为第0个分组）
  std::smatch sub_match{};
  if (regex_match(line, sub_match, patten)) {
    method_ = sub_match[1];
    path_ = sub_match[2];
    version_ = sub_match[3];
    state_ = HEADERS;
    return true;
  }
  // LOG_ERROR("RequestLine Error");
  return false;
}

void HttpRequest::ParseHeader(const std::string &line) {
  std::regex patten("^([^:]*): ?(.*)$");
  std::smatch sub_match;
  // eg:Content-Type: application/json
  if (regex_match(line, sub_match, patten)) {
    header_[sub_match[1]] = sub_match[2];
  } else {
    state_ = BODY;
  }
  // size_t pos = line.find(':');
  // if (pos != std::string::npos) {
  //   std::string key = line.substr(0, pos);
  //   std::string value = line.substr(pos + 2);  // Skip ': ' after colon
  //   header_[std::move(key)] = std::move(value);
  // } else {
  //   state_ = BODY;
  // }
}

void HttpRequest::ParseBody(const std::string &line) {
  body_ = line;
  ParsePost();
  state_ = FINISH;
  // LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

auto HttpRequest::ConverHex(char ch) -> int {
  if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  }
  return ch;
}

void HttpRequest::ParsePost() {
  // 根据请求头部的 Content-Type 字段来确定如何解析请求体的内容。如果是
  // application/x-www-form-urlencoded
  // 类型的请求，请求体内容通常是表单数据，需要进行解析和处理。如果是
  // multipart/form-data
  // 类型的请求，请求体内容通常是上传的文件数据，也需要进行相应的处理。
  // 此处只实现了 application/x-www-form-urlencoded  即注册和登录
  if (method_ == "POST" &&
      header_["Content-Type"] == "application/x-www-form-urlencoded") {
    ParseFromUrlencoded();
    if (DEFAULT_HTML_TAG.count(path_) != 0U) {
      int tag = DEFAULT_HTML_TAG.find(path_)->second;
      // LOG_DEBUG("Tag:%d", tag);
      if (tag == 0 || tag == 1) {
        bool is_login = (tag == 1);
        if (UserVerify(post_["username"], post_["password"], is_login)) {
          path_ = "/welcome.html";
          // 登陆（注册）成功，将路径重定向到欢迎页面（"/welcome.html"）
        } else {
          path_ = "/error.html";  // 登陆失败
        }
      }
    }
  }
}

void HttpRequest::ParseFromUrlencoded() {
  // 解析URL编码的POST请求体

  if (body_.empty()) {
    return;
  }
  std::string key;
  std::string value;
  int num = 0;  // 暂存解析十六进制字符时的数值
  int n = body_.size();
  int i = 0;
  int j = 0;

  for (; i < n; i++) {
    char ch = body_[i];
    switch (ch) {
      case '=':
        key = body_.substr(j, i - j);
        j = i + 1;
        break;
      case '+':  // 当字符是'+'时，表示空格，将其替换为实际的空格字符
        body_[i] = ' ';
        break;
      case '%':  // 当字符是'%'时，表示后面跟着两个十六进制字符，需要将其转换为实际的字符，将其转换为对应的数字并重新组合
        num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
        body_[i + 2] = num % 10 + '0';
        body_[i + 1] = num / 10 + '0';
        i += 2;
        break;
      case '&':  // 当字符是'&'时，表示键值对的结束，此时提取值，并将键值对存储到post_容器中
        value = body_.substr(j, i - j);
        j = i + 1;
        post_[key] = value;
        // LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
        break;
      default:
        break;
    }
  }
  assert(j <= i);
  if (post_.count(key) == 0 && j < i) {
    value = body_.substr(j, i - j);
    post_[key] = value;
    // 如果post_中还没有存储当前键值对，则存储当前键值对到post_容器中
  }
}

auto HttpRequest::UserVerify(const std::string_view name,
                             const std::string_view pwd,
                             const bool isLogin) -> bool {
  if (name.empty() || pwd.empty()) {
    return false;
  }
  // LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
  MYSQL *sql;
  SqlConnRAII sql_conn_raii(&sql, SqlConnPool::Instance());
  assert(sql);

  bool flag = false;
  char order[256] = {};      // 存储查询语句
  MYSQL_RES *res = nullptr;  // 查询结果集
  [[maybe_unused]] unsigned int j = 0;
  [[maybe_unused]] MYSQL_FIELD *fields = nullptr;  // 结果集中的字段

  if (!isLogin) {
    flag = true;
  }
  // 查询用户及密码
  snprintf(order, 256,
           "SELECT username, password FROM user WHERE username='%s' LIMIT 1",
           name.data());
  // snprintf将查询语句写入到order中
  // LOG_DEBUG("%s", order);

  if (mysql_query(sql, order) != 0) {
    mysql_free_result(res);  // 查询失败则释放结果集并返回false
    return false;
  }
  res = mysql_store_result(sql);     // 结果集--行
  j = mysql_num_fields(res);         // 列数
  fields = mysql_fetch_fields(res);  // (列)

  while (MYSQL_ROW row = mysql_fetch_row(res)) {
    // LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
    std::string_view password(row[1]);
    // 登录
    if (isLogin) {
      flag = (pwd == password);
      // if (!flag) {
      //   // LOG_DEBUG("pwd error!");
      // }
      break;
    }
    flag = true;
    // flag=false;
    break;
    //  LOG_DEBUG("user used!");
  }
  mysql_free_result(res);  // 释放结果集

  // 注册行为 且 用户名未被使用
  if (!isLogin && flag) {
    // LOG_DEBUG("regirster!");
    memset(order, 0, 256);
    snprintf(order, 256,
             "INSERT INTO user(username, password) VALUES('%s','%s')",
             name.data(), pwd.data());
    // LOG_DEBUG("%s", order);
    if (mysql_query(sql, order) != 0) {
      // LOG_DEBUG("Insert error!");
      flag = false;
    }
  }
  // SqlConnPool::Instance()->FreeConn(sql);  //RAII  通过sql_conn_raii释放
  // LOG_DEBUG("UserVerify success!!");
  return flag;
}  // 注册or登录成功都返回true

auto HttpRequest::Path() const -> std::string { return path_; }

auto HttpRequest::Path() -> std::string & { return path_; }
auto HttpRequest::Method() const -> std::string { return method_; }

auto HttpRequest::Version() const -> std::string { return version_; }

auto HttpRequest::GetPost(const std::string &key) const -> std::string {
  assert(!key.empty());
  if (post_.count(key) == 1) {
    return post_.find(key)->second;
  }
  return "";
}

auto HttpRequest::GetPost(const char *key) const -> std::string {
  assert(key != nullptr);
  if (post_.count(key) == 1) {
    return post_.find(key)->second;
  }
  return "";
}