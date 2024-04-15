# include "sqlconnpool.h"

// 初始化数据库池
void SqlConnPool::Init(const char *host, const char *user, const char *passwd, const char *db, unsigned int port, int conn_size){
    for(int i = 0; i < conn_size; ++ i) {
        MYSQL *conn = nullptr;
        conn = mysql_init(conn);
        if(!conn) {
            LOG_ERROR("MySql init error!");
            assert(conn);
        }
        conn = mysql_real_connect(conn, host, user, passwd, db, port, nullptr, 0);
        if(!conn) {
            LOG_ERROR("MySql Connect error!");
            assert(conn);
        }
        sql_queue_.push(conn);
    }
    if(sem_init(&sem_, 0, conn_size) == -1) {
        LOG_ERROR("MySql sem init error!");
        assert(0);
    }
    max_conn_ = conn_size;
}
    
void SqlConnPool::ClosePool(){
    std::lock_guard<std::mutex> locker(mtx_);
    if(static_cast<int>(sql_queue_.size()) != max_conn_) {
        // log
        LOG_WARN("SqlConnPool is runing!");
        assert(static_cast<int>(sql_queue_.size()) == max_conn_);
    }
    while(!sql_queue_.empty()) {
        mysql_close(sql_queue_.front());
        sql_queue_.pop();
    }
    mysql_library_end();
}

// 获取一个连接
MYSQL *SqlConnPool::GetSqlConn(){
    MYSQL *conn = nullptr;
    if(sql_queue_.empty()) {
        LOG_WARN("SqlConnPool busy!");
    }
    sem_wait(&sem_);
    std::lock_guard<std::mutex> locker(mtx_);
    conn = sql_queue_.front();
    sql_queue_.pop();
    return conn;
}

// 释放一个连接
void SqlConnPool::FreeSqlConn(MYSQL *conn){
    assert(conn);
    std::lock_guard<std::mutex> locker(mtx_);
    sql_queue_.push(conn);
    sem_post(&sem_);
}
// 获取空闲连接数量
int SqlConnPool::GetFreeConnCount(){
    std::lock_guard<std::mutex> locker(mtx_);
    return sql_queue_.size();
}