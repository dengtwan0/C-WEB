/*
    只处理响应头，如有文件在外部逻辑写
*/
# ifndef WAN_HTTP_RESPONSE_H
# define WAN_HTTP_RESPONSE_H

# include <unordered_map>
# include <fcntl.h>       // open
# include <unistd.h>      // close
# include <sys/stat.h>    // stat
# include <sys/mman.h>    // mmap, munmap
#include <sys/uio.h>     

# include "../buffer/buffer.h"
# include "../log/log.h"
# include "httpfile.h"

class HttpResponse {

public:
    HttpResponse() {
        code_ = -1;
        is_keep_alive_ = false;
        file_stat_ptr_ = nullptr;
    }
    ~HttpResponse() = default;

    void Init(bool is_keep_alive, int code, HttpFile::FileStat *file_stat) {
        is_keep_alive_ = is_keep_alive;
        code_ = code;
        file_stat_ptr_ = file_stat;
    }

    void MakeResponse(Buffer &buffer, struct iovec *iov, int &iov_cnt);

    int Code() const { return code_; }

private:
    bool is_keep_alive_ = false;
    int code_ = -1;
    HttpFile::FileStat *file_stat_ptr_ = nullptr;

    void AddStatusLine(Buffer &buffer);
    void AddHeader(Buffer &buffer);
    void AddContent(Buffer &buffer);
    void ErrorContent(Buffer& buffer, std::string message);
    
    static const std::unordered_map<int, std::string> CODE_STATUS;          // 编码状态集

};



# endif