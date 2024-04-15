# include "httpconn.h"

const char* HttpConn::srcDir_;
std::atomic<int> HttpConn::clientCnt_;
bool HttpConn::isEt_;

void HttpConn::Init(int client_fd, const struct sockaddr_in* addr_ptr) {
    assert(client_fd > 0);
    clientCnt_ ++;
    client_fd_ = client_fd;
    addr_ptr_ = addr_ptr;
    read_buff_.RetrieveAll();
    write_buff_.RetrieveAll();
    is_init_ = true;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", client_fd_, GetIP(), GetPort(), (int)clientCnt_);
}

void HttpConn::InitBuffer() {
    read_buff_.RetrieveAll();
    write_buff_.RetrieveAll();
}

void HttpConn::Close() {
    file_.UnmapFile();
    if(is_init_) {
        is_init_ = false; 
        clientCnt_--;
        close(client_fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", client_fd_, GetIP(), GetPort(), (int)clientCnt_);
    }
}

// 读入socket
ssize_t HttpConn::Read(int *readErrno){
    ssize_t len = 0;
    do {
        ssize_t len_t = read_buff_.ReadFd(client_fd_, readErrno);
        // len_t为0说明读取时在文件末尾，可能刚好第三次read只剩'\0'
        if(len_t < 0) 
            return len_t;
        len += len_t;
    } while(isEt_);
    return len;
}

// 读不完整返回false，解析完成返回true,状态码存在read_ret_中
bool HttpConn::ReadProcess(){
    request_.Init(srcDir_);
    if(read_buff_.ReadableBytes() <= 0) {
        return false;
    }
    read_ret_ = request_.parse(read_buff_);
    if(read_ret_ == NO_REQUEST) return false;
    return true;
}

// 请求成功，根据状态码写响应报文
bool HttpConn::WriteProcess(){

    // 每个错误码都对应一个html文件，如果文件打不开或映射错误，GetFileStatPtr为nullptr
    file_.Init(srcDir_, request_.path());
    
    switch (read_ret_) {
        case INTERNAL_ERROR: // 服务器端错误
        {
            response_.Init(false, 500, file_.GetFileStatPtr());
            break;
        }
        case NO_RESOURCE:
        {
            response_.Init(false, 404, file_.GetFileStatPtr());
            break;
        }
        case BAD_REQUEST:
        {
            response_.Init(false, 400, file_.GetFileStatPtr());
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            response_.Init(false, 403, file_.GetFileStatPtr());
            break;
        }
        case FILE_REQUEST: // 文件存在，一定会直接return
        {
            response_.Init(IsKeepAlive(), 200, file_.GetFileStatPtr());
            break;
        }
        default:
            return false;
    }
    response_.MakeResponse(write_buff_, &iov_[0], iov_cnt_); // 生成响应报文放入writeBuff_中
    bytes_to_send_ = iov_[0].iov_len + iov_[1].iov_len;

    LOG_DEBUG("filesize:%d, %d  to %d", file_.FileLen() , iov_cnt_, ToWriteBytes());
    return true;
}

// 写入socket，当正确写完，会自动初始化buffer，如果写错误，返回<0关闭连接
ssize_t HttpConn::Write(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = write_buff_.WriteFd(client_fd_, iov_, iov_cnt_, saveErrno);   // 将iov的内容写到fd中
        if(len < 0) {
            break;
        }
        bytes_to_send_ -= len;
        if(bytes_to_send_ == 0) {
            assert(iov_[0].iov_len + iov_[1].iov_len  == 0);
            break; 
        } /* 传输结束 */
    } while(isEt_ || ToWriteBytes() > 10240);
    return len;
}