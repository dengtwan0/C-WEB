# ifndef WAN_WEVSERVER_H
# define WAN_WEVSERVER_H

# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#include <errno.h>

#include <unordered_map>
#include <fcntl.h>       // fcntl()
# include <assert.h>
#include <unistd.h>      // close()

# include "epoller.h"
# include "../pool/sqlconnpool.h"
# include "../pool/threadpool.h"
# include "../http/httpconn.h"
# include "../buffer/buffer.h"
# include "../log/log.h"
# include "../timer/heaptimer.h"

class WebServer {
public:
    WebServer(int port, int timeout_ms, int trig_mode, int actor_model,
              const char *sql_host, unsigned int sql_port, const char *sql_user, const char *sql_passwd, const char *sql_db,
              int sql_conn_size, int thread_number, 
              bool openLog, bool is_async, int log_level, int log_size)
    : port_(port), actor_model_(actor_model), timeout_ms_(timeout_ms), stop_server_(false), 
    epoller_(std::make_unique<Epoller>(1024)), 
    thread_pool_(std::make_unique<ThreadPool>(thread_number, 10000)),
    timer_(std::make_unique<HeapTimer>())
    {
        // 初始化静态变量
        srcDir_ = getcwd(nullptr, 256);
        assert(srcDir_);
        strcat(srcDir_, "/resources/");
        HttpConn::clientCnt_ = 0;
        HttpConn::srcDir_ = srcDir_;
        // 初始日志
        if(openLog) {
            Log::GetInstance() -> Init(log_level, "./log", ".log", is_async, log_size);
            if(stop_server_) { LOG_ERROR("========== Server init error!=========="); }
            else {
                LOG_INFO("========== Server init ==========");
                LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                                (listen_event_ & EPOLLET ? "ET": "LT"),
                                (conn_event_ & EPOLLET ? "ET": "LT"));
                LOG_INFO("LogSys level: %d", log_level);
                LOG_INFO("srcDir: %s", HttpConn::srcDir_);
                LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", sql_conn_size, thread_number);
            }
        }
        // 数据库连接池
        SqlConnPool::GetInstance() -> Init(sql_host, sql_user, sql_passwd, sql_db, sql_port, sql_conn_size);

        // 初始化事件和监听（ETorLT）
        InitTrigMode(trig_mode);
        // 创建监听socket开始监听
        if(!EventListen()) { stop_server_ = true; }
    }
    ~WebServer() {
        // 关闭监听socket
        close(listen_fd_);
        stop_server_ = true;
        // 关闭数据库池
        SqlConnPool::GetInstance() -> ClosePool();
        free(srcDir_);
        // epoller由析构函数释放epfd
        // 客户连接：主动释放client->close();析构释放：调用close()
        // 线程池析构释放，唤醒所有子线程
    }
    void EventLoop();
private:
    static const int MAX_FD = 65536;

    bool stop_server_ = false;
    char* srcDir_;

    uint32_t listen_event_;
    uint32_t conn_event_;
    int actor_model_ = 0;
    void InitTrigMode(int trig_mode);
    

    int listen_fd_;
    int port_;
    bool EventListen();

    int timeout_ms_;

    void CloseConn(HttpConn *client);
    void DealListen();
    void AddClient(int fd, sockaddr_in* addr_ptr);
    void DealRead(HttpConn *client);
    void DealWrite(HttpConn *client);
    // void DealSignal(bool& timeout, bool& stop_server);

    void SendError(int fd, const char* info);
    void ExtentTime(HttpConn *client);

    // void ReactorRead(HttpConn *client);
    void OnRead(HttpConn *client);
    // void ReactorWrite(HttpConn *client);
    void OnWrite(HttpConn *client);
    void OnProcess(HttpConn *client);

    std::unique_ptr<Epoller> epoller_;
    std::unique_ptr<ThreadPool> thread_pool_;
    std::unique_ptr<HeapTimer> timer_;
    std::unordered_map<int, HttpConn> clients_;

};

# endif