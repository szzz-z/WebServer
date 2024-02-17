#include "sqlconnpool.h"
#include <cassert>

SqlConnPool::SqlConnPool() {
  use_count_ = 0;
  free_count_ = 0;
}

auto SqlConnPool::Instance() -> SqlConnPool * {
  static SqlConnPool conn_pool;
  return &conn_pool;
}

void SqlConnPool::Init(const char *host, int port, const char *user, const char *pwd, const char *dbName,
                       int connSize = 10) {
  assert(connSize > 0);
  for (int i = 0; i < connSize; i++) {
    MYSQL *sql = nullptr;
    sql = mysql_init(sql);
    if (sql == nullptr) {
      // LOG_ERROR("MySql init error!");
      assert(sql);
    }
    sql = mysql_real_connect(sql, host, user, pwd, dbName, port, nullptr, 0);
    if (sql == nullptr) {
      // LOG_ERROR("MySql Connect error!");
    }
    conn_que_.push(sql);
  }
  max_conn_ = connSize;
  sem_init(&sem_id_, 0, max_conn_);
}

auto SqlConnPool::GetConn() -> MYSQL * {
  MYSQL *sql = nullptr;
  if (conn_que_.empty()) {
    // LOG_WARN("SqlConnPool busy!");
    return nullptr;
  }
  sem_wait(&sem_id_);
  {
    std::lock_guard<std::mutex> lock(mtx_);
    sql = conn_que_.front();
    conn_que_.pop();
  }
  return sql;
}

void SqlConnPool::FreeConn(MYSQL *conn) {
  assert(conn);
  std::lock_guard<std::mutex> lock(mtx_);
  conn_que_.push(conn);
  sem_post(&sem_id_);
}

void SqlConnPool::ClosePool() {
  std::lock_guard<std::mutex> lock(mtx_);
  while (!conn_que_.empty()) {
    auto item = conn_que_.front();
    conn_que_.pop();
    mysql_close(item);
  }
  mysql_library_end();
}

auto SqlConnPool::GetFreeConnCount() -> int {
  std::lock_guard<std::mutex> lock(mtx_);
  return conn_que_.size();
}

SqlConnPool::~SqlConnPool() { ClosePool(); }
