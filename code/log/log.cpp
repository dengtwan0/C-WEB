# include "log.h"

Log::Log() : fp_(nullptr), block_queue_ptr_(nullptr), write_thread_prt_(nullptr), 
    is_async_(false), line_cnt_(0), is_open_(false) {

}

Log::~Log() {
    while(!block_queue_ptr_ -> empty()) {
        block_queue_ptr_ -> flush();
    }
    block_queue_ptr_ -> close();
    write_thread_prt_ -> join();
    if(fp_) {
        std::lock_guard<std::mutex> locker(mtx_);
        Flush();
        fclose(fp_);
    }
}

void Log::Init(int level, const char* path, const char* suffix, bool is_async, int maxQueueCapacity) {
    level_ = level;
    path_ = path;
    suffix_ = suffix;
    is_async_ = is_async;
    if(is_async_) {
        assert(maxQueueCapacity > 0);
        if(!block_queue_ptr_) {
            block_queue_ptr_ = std::make_unique<BlockQueue<std::string>>(maxQueueCapacity);
            write_thread_prt_ = std::make_unique<std::thread>(FlushLogThread);
        }
    }

    line_cnt_ = 0;

    time_t timer = time(nullptr);
    struct tm *l_time = localtime(&timer);

    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
            path_, l_time -> tm_year + 1900, l_time -> tm_mon + 1, l_time -> tm_mday, suffix_);
    today_ = l_time -> tm_mday;
    
    // 准备日志文件
    {
        std::lock_guard<std::mutex> locker(mtx_);
        buffer_.RetrieveAll();
        if(fp_) {   // 重新打开
            Flush();
            fclose(fp_);
        }
        fp_ = fopen(fileName, "a"); // 打开文件读取并附加写入
        // path文件不存在
        if(fp_ == nullptr) {
            mkdir(path_, 0777);
            fp_ = fopen(fileName, "a"); // 生成目录文件（最大权限）
        }
        assert(fp_ != nullptr);
        is_open_ = true;
    }
}

void Log::WriteLog(int level, const char *format, ...) {
    time_t timer = time(nullptr);
    struct tm *l_time = localtime(&timer);
    va_list va;

    // 当不是今天或者行数超了，创建新的log文件
    if(today_ != l_time -> tm_mday || (line_cnt_ && (line_cnt_ % MAX_LINES == 0))) {
        std::unique_lock<std::mutex> locker(mtx_);
        locker.unlock();

        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", l_time -> tm_year + 1900, l_time -> tm_mon + 1, l_time -> tm_mday);

        if (today_ != l_time -> tm_mday)    // 时间不匹配，则替换为最新的日志文件名
        {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            today_ = l_time -> tm_mday;
            line_cnt_ = 0;
        }
        else {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (line_cnt_  / MAX_LINES), suffix_);
        }

        locker.lock();
        Flush();
        fclose(fp_);
        fp_ = fopen(newFile, "a");
        assert(fp_ != nullptr);
    }

    // 往buffer内写日志
    {
        std::unique_lock<std::mutex> locker(mtx_);
        line_cnt_ ++;

        buffer_.EnsureWriteable(128);

        int n = snprintf(buffer_.BeginWrite(), 256, "%d-%02d-%02d %02d:%02d:%02d ",
                    l_time -> tm_year + 1900, l_time -> tm_mon + 1, l_time -> tm_mday,
                    l_time -> tm_hour, l_time -> tm_min, l_time -> tm_sec);
        // n不包括自动加的'\0'，后续写入覆盖该字符
        buffer_.HasWritten(n);
        AppendLogLevelTitle(level);    

        va_start(va, format);
        int m = vsnprintf(buffer_.BeginWrite(), buffer_.WritableBytes(), format, va);
        va_end(va);
        buffer_.HasWritten(m);
        buffer_.Append("\n\0", 2);

        if(is_async_ && block_queue_ptr_ && !block_queue_ptr_ -> full()) {
            block_queue_ptr_ -> push(buffer_.RetrieveAllToStr());
        }
        else {
            fputs(buffer_.Peek(), fp_);
        }
        buffer_.RetrieveAll();
    }
}
// 添加日志等级
void Log::AppendLogLevelTitle(int level) {
    switch(level) {
    case 0:
        buffer_.Append("[debug]: ", 9);
        break;
    case 1:
        buffer_.Append("[info] : ", 9);
        break;
    case 2:
        buffer_.Append("[warn] : ", 9);
        break;
    case 3:
        buffer_.Append("[error]: ", 9);
        break;
    default:
        buffer_.Append("[info] : ", 9);
        break;
    }
}
void Log::Flush() {
    if(is_async_) {  // 只有异步日志才会用到deque
        block_queue_ptr_ -> flush();
    }
    fflush(fp_);    // 清空输入缓冲区
}

void Log::AsyncWrite() {
    std::string str = "";
    while(block_queue_ptr_ -> pop(str)) {
        std::lock_guard<std::mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}