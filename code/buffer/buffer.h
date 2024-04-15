# ifndef WAN_MUDUO_BUFFER_H
# define WAN_MUDUO_BUFFER_H

#include <cstring>   //perror
#include <iostream>
#include <unistd.h>  // write
#include <sys/uio.h> //readv
#include <vector> //readv
#include <atomic>
#include <assert.h>

static const size_t kPrePendIndex = 8;
static const size_t kInitialSize = 1024;

class Buffer {
public:
    Buffer() : buffer_(kPrePendIndex + kInitialSize), read_index_(kPrePendIndex), write_index_(kPrePendIndex) {}
    ~Buffer() {};

    size_t WritableBytes() const { return buffer_.size() - read_index_; }
    size_t ReadableBytes() const { return write_index_ - read_index_; }
    size_t PrependableBytes() const { return read_index_; }

    // 写开始位置
    char* BeginWrite() { return &buffer_[write_index_]; }
    const char* BeginWrite() const { return &buffer_[write_index_]; }
    // 读开始位置
    char* Peek() { return &buffer_[read_index_]; }
    const char* Peek() const { return &buffer_[read_index_]; }
    
    // 将数据从socket写入buffer
    ssize_t ReadFd(int fd, int *Errno);
    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void EnsureWriteable(size_t len);
    
    // 从buffer读出数据
    void Retrieve(size_t len) {
        if(read_index_ + len != write_index_)
            read_index_ += len;
        else RetrieveAll();
    }
    void RetrieveAll() {
        bzero(&buffer_[0], buffer_.size()); // 覆盖原本数据
        read_index_ = kPrePendIndex;
        write_index_ = read_index_;
    }
    void RetrieveUntil(const char* end) {
        Retrieve(end - Peek());
    }
    std::string RetrieveAllToStr() {
        std::string str(Peek(), BeginWrite());
        RetrieveAll();
        return str;
    }
    std::string RetrieveToStr(size_t len) {
        if(read_index_ + len > write_index_) {
            len = write_index_ - read_index_;
            std::cout << "长度超出可读范围，读取所有可读内容" << std::endl;
        }
        std::string str(Peek(), Peek() + len);
        Retrieve(len);
        return str;
    }
    void HasWritten(size_t len) {
        write_index_ += len;
    }

    // 将iov数据写入fd
    ssize_t WriteFd(int fd, struct iovec *iov, const int iov_cnt, int *Errno);

private:
    std::vector<char> buffer_;  
    size_t read_index_;  // 读的下标
    size_t write_index_; // 写的下标
};



# endif