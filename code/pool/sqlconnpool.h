# ifndef WAN_SQLCONNPOOL_H
# define WAN_SQLCONNPOOL_H

# include <mysql/mysql.h>
# include <mutex>
# include <queue>
# include <assert.h>
# include <semaphore.h>
#include "../log/log.h"

// 懒汉模式
class SqlConnPool {
public:
    SqlConnPool(const SqlConnPool &sql_pool_) = delete;
    SqlConnPool& operator = (const SqlConnPool &sql_pool_) = delete;

    // 获取实例
    static SqlConnPool* GetInstance() {
        static SqlConnPool sql_pool_;
        return &sql_pool_;
    }
    // 初始化数据库池
    void Init(const char *host, const char *user, 
              const char *passwd, const char *db, 
              unsigned int port, int conn_size);
    void ClosePool();

    // 获取一个连接
    MYSQL *GetSqlConn();
    // 释放一个连接
    void FreeSqlConn(MYSQL *conn);
    // 获取空闲连接数量
    int GetFreeConnCount();

private:
    SqlConnPool() = default;
    ~SqlConnPool() { ClosePool(); }

    int max_conn_;

    std::queue<MYSQL *> sql_queue_;
    std::mutex mtx_;
    sem_t sem_;
};

/* 资源在对象构造初始化 资源在对象析构时释放*/
class SqlConnRAII {
public:
    SqlConnRAII(MYSQL** sql, SqlConnPool *connpool) {
        assert(connpool);
        *sql = connpool -> GetSqlConn();
        sql_ = *sql;
        connpool_ = connpool;
    }
    
    ~SqlConnRAII() {
        if(sql_) { connpool_ -> FreeSqlConn(sql_); }
    }
    
private:
    MYSQL *sql_;
    SqlConnPool* connpool_;
};

# endif