# ifndef WAN_HTTP_CONN_H
# define WAN_HTTP_CONN_H


# include <sys/uio.h>     // readv/writev
# include <unistd.h>      // close()
#include <arpa/inet.h>   // sockaddr_in
# include <errno.h>   
#include <stdlib.h>      // atoi()

# include "httprequest.h"
# include "httpfile.h"
# include "httpresponse.h"
# include "../buffer/buffer.h"
#include "../log/log.h"

class HttpConn {
public:
    HttpConn() {
        client_fd_ = -1;
        is_init_ = false;
        bytes_to_send_ = 0;
        assert(iov_[0].iov_len == 0 && iov_[1].iov_len == 0);
    }
    ~HttpConn() { Close(); }
    void Close();

    void Init(int client_fd, const struct sockaddr_in* addr_ptr);
    void InitBuffer();

    int GetFd() const { return client_fd_; }
    int GetPort() const { return addr_ptr_ -> sin_port; }
    const char* GetIP() const { return inet_ntoa(addr_ptr_ -> sin_addr); }
    const sockaddr_in* GetAddrPtr() const {return addr_ptr_;}
    int ToWriteBytes() {
        assert(bytes_to_send_ == (iov_[0].iov_len + iov_[1].iov_len));
        return static_cast<int>(bytes_to_send_);
    }
    bool IsKeepAlive() const {
        return request_.IsKeepAlive();
    }

    // 读入socket
    ssize_t Read(int *readErrno);
    // 读不完整返回false，写准备完成返回true,状态码存在read_ret_中
    bool ReadProcess();
    bool WriteProcess();
    // 写入socket
    ssize_t Write(int* saveErrno);

    static bool isEt_;
    static const char* srcDir_;
    static std::atomic<int> clientCnt_;  // 原子，支持锁
    
private:
    int client_fd_;
    const struct sockaddr_in * addr_ptr_ = nullptr;
    bool is_init_ = false; // 调用init绑定client_fd后为true，析构时需要close(fd)

    size_t bytes_to_send_;
    int iov_cnt_ = 1;
    struct iovec iov_[2] = {0};

    // process
    HTTP_CODE read_ret_;
    Buffer read_buff_; // 读缓冲区
    Buffer write_buff_; // 写缓冲区

    HttpRequest request_;
    HttpResponse response_;
    HttpFile file_;
};


# endif