#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <semaphore.h>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include "../log/log.h"

class SqlConnPool {
 public:
  static auto Instance() -> SqlConnPool *;

  auto GetConn() -> MYSQL *;
  void FreeConn(MYSQL *conn);
  auto GetFreeConnCount() -> int;

  void Init(const char *host, int port, const char *user, const char *pwd, const char *dbName, int connSize);
  void ClosePool();

 private:
  SqlConnPool();
  ~SqlConnPool();

  int max_conn_;
  int use_count_;
  int free_count_;

  std::queue<MYSQL *> conn_que_;
  std::mutex mtx_;
  sem_t sem_id_;
};

#endif  // SQLCONNPOOL_H