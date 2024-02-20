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
    return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
  }
  return false;
}

auto HttpRequest::Parse(Buffer &buff) -> bool {
  const char crlf[] = "\r\n";
  if (buff.ReadableBytes() <= 0) {
    return false;
  }
  while ((buff.ReadableBytes() != 0U) && state_ != FINISH) {
    const char *line_end = std::search(buff.Peek(), buff.BeginWriteConst(), crlf, crlf + 2);
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
    buff.RetrieveUntil(line_end + 2);
  }
  // LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
  return true;
}

void HttpRequest::ParsePath() {
  if (path_ == "/") {
    path_ = "/index.html";
  } else {
    for (auto &item : DEFAULT_HTML) {
      if (item == path_) {
        path_ += ".html";
        break;
      }
    }
  }
}

auto HttpRequest::ParseRequestLine(const std::string &line) -> bool {
  std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
  std::smatch sub_match;
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
  if (regex_match(line, sub_match, patten)) {
    header_[sub_match[1]] = sub_match[2];
  } else {
    state_ = BODY;
  }
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
  if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
    ParseFromUrlencoded();
    if (DEFAULT_HTML_TAG.count(path_) != 0U) {
      int tag = DEFAULT_HTML_TAG.find(path_)->second;
      // LOG_DEBUG("Tag:%d", tag);
      if (tag == 0 || tag == 1) {
        bool is_login = (tag == 1);
        if (UserVerify(post_["username"], post_["password"], is_login)) {
          path_ = "/welcome.html";
        } else {
          path_ = "/error.html";
        }
      }
    }
  }
}

void HttpRequest::ParseFromUrlencoded() {
  if (body_.empty()) {
    return;
  }

  std::string key;
  std::string value;
  int num = 0;
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
      case '+':
        body_[i] = ' ';
        break;
      case '%':
        num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
        body_[i + 2] = num % 10 + '0';
        body_[i + 1] = num / 10 + '0';
        i += 2;
        break;
      case '&':
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
  }
}

auto HttpRequest::UserVerify(const std::string &name, const std::string &pwd, bool isLogin) -> bool {
  if (name.empty() || pwd.empty()) {
    return false;
  }
  // LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
  MYSQL *sql;
  SqlConnRAII sql_conn_raii(&sql, SqlConnPool::Instance());
  assert(sql);

  bool flag = false;
  [[maybe_unused]] unsigned int j = 0;
  char order[256] = {0};
  [[maybe_unused]] MYSQL_FIELD *fields = nullptr;
  MYSQL_RES *res = nullptr;

  if (!isLogin) {
    flag = true;
  }
  /* 查询用户及密码 */
  snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
  // LOG_DEBUG("%s", order);

  if (mysql_query(sql, order) != 0) {
    mysql_free_result(res);
    return false;
  }
  res = mysql_store_result(sql);
  j = mysql_num_fields(res);
  fields = mysql_fetch_fields(res);

  while (MYSQL_ROW row = mysql_fetch_row(res)) {
    // LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
    std::string password(row[1]);
    /* 注册行为 且 用户名未被使用*/
    if (isLogin) {
      flag = (pwd == password);
      if (!flag) {
        // LOG_DEBUG("pwd error!");
      }
      break;
    }
    flag = false;
    break;
    //  LOG_DEBUG("user used!");
  }
  mysql_free_result(res);

  /* 注册行为 且 用户名未被使用*/
  if (!isLogin && flag) {
    // LOG_DEBUG("regirster!");
    memset(order, 0, 256);
    snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
    // LOG_DEBUG("%s", order);
    if (mysql_query(sql, order) != 0) {
      // LOG_DEBUG("Insert error!");
      flag = false;
    }
  }
  SqlConnPool::Instance()->FreeConn(sql);
  // LOG_DEBUG("UserVerify success!!");
  return flag;
}

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