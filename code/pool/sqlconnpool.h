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
  static auto Instance() -> SqlConnPool *;  // 单例模式 确保只创建一个pool

  auto GetConn() -> MYSQL *;       // 从pool里获取一个可用的conn
  void FreeConn(MYSQL *conn);      // 将用完的conn返回pool
  auto GetFreeConnCount() -> int;  // 空闲conn的数量

  void Init(const char *host, int port, const char *user, const char *pwd,
            const char *dbName,
            int connSize);  // 初始化连接池，创建指定数量的数据库连接。
  void ClosePool();  // 释放所有资源

 private:
  SqlConnPool();
  ~SqlConnPool();

  int max_conn_;    // 最大conn数
  int use_count_;   // 已用conn数
  int free_count_;  // 空闲conn数

  std::queue<MYSQL *> conn_que_;
  std::mutex mtx_;
  sem_t sem_id_;  // 信号量
};

#endif  // SQLCONNPOOL_H