#ifndef WAN_LOG_H
#define WAN_LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>           // vastart va_end
#include <assert.h>
#include <sys/stat.h>         // mkdir
#include "blockqueue.h"
#include "../buffer/buffer.h"


class Log {
public:
    Log(const Log &log) = delete;
    Log & operator = (const Log &log) = delete;

    static Log* GetInstance() {
        static Log log;
        return &log;
    }

    static void FlushLogThread() {
        Log::GetInstance() -> AsyncWrite();
    }

    // 初始化日志实例（日志等级，日志保存路径，日志文件后缀，阻塞队列最大容量）
    void Init(int level, const char* path = "./log", const char* suffix =".log",
              bool is_async = false, int maxQueueCapacity = 1024);

    void WriteLog(int level, const char *format, ...);
    void AppendLogLevelTitle(int level);
    void Flush();


    int GetLevel() {
        std::lock_guard<std::mutex> locker(mtx_);
        return level_;
    }
    void SetLevel(int level) {
        std::lock_guard<std::mutex> locker(mtx_);
        level_ = level;
    }
    bool IsOpen() { return is_open_; }


private:
    Log();
    ~Log();
    
    void AsyncWrite(); // 异步写日志方法

    static const int LOG_PATH_LEN = 256;    // 日志文件最长文件名
    static const int LOG_NAME_LEN = 256;    // 日志最长名字
    static const int MAX_LINES = 50000;     // 日志文件内的最长日志条数

    int level_; // 日志等级，只有大于等于的日志会输出
    const char* path_;
    const char* suffix_;

    bool is_open_;
    bool is_async_;      // 是否开启异步日志

    int line_cnt_;
    int today_;
    
    std::mutex mtx_;

    FILE *fp_; //打开log的文件指针
    Buffer buffer_; // 输出缓冲区
    std::unique_ptr<BlockQueue<std::string>> block_queue_ptr_; //阻塞队列
    std::unique_ptr<std::thread> write_thread_prt_; //写线程的指针
};



# define LOG_BASE(level, format, ...) \
    do {\
        Log *log = Log::GetInstance();\
        if(log -> IsOpen() && log -> GetLevel() <= level) {\
            log -> WriteLog(level, format, ##__VA_ARGS__);\
            log -> Flush();\
        }\
    } while(0);


# define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
# define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
# define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
# define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);



# endif