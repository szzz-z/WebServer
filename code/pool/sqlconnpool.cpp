#include "sqlconnpool.h"

#include <cassert>

SqlConnPool::SqlConnPool() {
  use_count_ = 0;
  free_count_ = 0;
}

auto SqlConnPool::Instance() -> SqlConnPool * {
  static SqlConnPool conn_pool;
  return &conn_pool;  // 使用局部静态变量实现单例模式，确保全局唯一的实例，并返回实例的指针
}

void SqlConnPool::Init(const char *host, int port, const char *user,
                       const char *pwd, const char *dbName, int connSize = 10) {
  assert(connSize > 0);
  for (int i = 0; i < connSize; i++) {
    MYSQL *sql = nullptr;  // 一个MYSQL对象在MySQL C API中
    // 代表一个到MySQL数据库服务器的连接。

    sql = mysql_init(sql);
    // 初始化一个MYSQL对象，用于后续的连接操作。
    // 如果传入的MYSQL指针是nullptr，这个函数会分配、初始化一个新的对象，并返回这个对象的指针。
    if (sql == nullptr) {
      // LOG_ERROR("MySql init error!");
      assert(sql);
    }
    sql = mysql_real_connect(sql, host, user, pwd, dbName, port, nullptr, 0);
    // 用于建立到数据库的连接。它需要传入通过mysql_init初始化的MYSQL对象，
    // 数据库服务器的地址（host）、用户名（user）、密码（pwd）、数据库名（dbName）、端口号（port），
    // 以及其他可选的参数如UNIX套接字和客户端标志。
    // 如果连接成功，返回MYSQL对象的指针；如果失败，返回nullptr。

    if (sql == nullptr) {
      // LOG_ERROR("MySql Connect error!");
    }
    conn_que_.push(sql);
  }
  max_conn_ = connSize;
  sem_init(&sem_id_, 0, max_conn_);
  // pashred指示信号量是在进程间共享还是线程间共享。
  // 如果pshared的值为0，信号量将只能被初始化它的进程的所有线程共享；
  // 如果pshared的值非0，信号量可以在多个进程间共享。
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
  mysql_library_end();  // 在程序结束时调用，清理分配给MySQL客户端库的资源。
}

auto SqlConnPool::GetFreeConnCount() -> int {
  std::lock_guard<std::mutex> lock(mtx_);
  return conn_que_.size();
}

SqlConnPool::~SqlConnPool() { ClosePool(); }
