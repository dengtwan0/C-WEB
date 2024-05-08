# include "buffer.h"


ssize_t Buffer::ReadFd(int fd, int *Errno) {
    char buff[65535];
    struct iovec iov[2];

    size_t writeable_bytes = WritableBytes();
    iov[0].iov_base = BeginWrite(); 
    iov[0].iov_len = writeable_bytes;
    iov[1].iov_base = buff; // 因为是一维数组，可以用buff作为首地址
    iov[1].iov_len = sizeof(buff);
    
    ssize_t len = readv(fd, iov, 2);
    if(len < 0) {
        *Errno = errno;
    }
    else if(static_cast<size_t>(len) <= writeable_bytes) {
        write_index_ += len;
    }
    else {
        write_index_ = buffer_.size();
        Append(buff, static_cast<size_t>(len - writeable_bytes));
    }
    return len;
}

void Buffer::Append(const char* str, size_t len) {
    EnsureWriteable(len);
    std::copy(str, str + len, BeginWrite()); // 从[str, str+len）拷贝到buffer，需要buffer有足够空间
    write_index_ += len;
}
void Buffer::Append(const std::string& str) {
    Append(str.c_str(), str.size());
}

void Buffer::EnsureWriteable(size_t len) {
    size_t writable_bytes = WritableBytes();
    if(len <= writable_bytes) return;
    if(writable_bytes + PrependableBytes() >= len + kPrePendIndex) {
        size_t readable_bytes = ReadableBytes();
        std::copy(Peek(), BeginWrite(), &buffer_[0] + kPrePendIndex);
        write_index_ = readable_bytes + kPrePendIndex;
        read_index_ = kPrePendIndex;
        assert(writable_bytes == ReadableBytes());
    }
    else {
        buffer_.resize(buffer_.size() + len + 1);
    }
}

ssize_t Buffer::WriteFd(int fd, struct iovec *iov, const int iov_cnt, int *Errno) {
    ssize_t len = writev(fd, iov, iov_cnt);
    if(len < 0) {
        *Errno = errno;
    }
    else if(static_cast<size_t>(len) > iov[0].iov_len) {
        iov[1].iov_base = (uint8_t*) iov[1].iov_base + (len - iov[0].iov_len);
        iov[1].iov_len -= (len - iov[0].iov_len);
        if(iov[0].iov_len) {
            RetrieveAll();
            iov[0].iov_len = 0;
        }
    }
    else {
        iov[0].iov_base = (uint8_t*) iov[0].iov_base + len;
        iov[0].iov_len -= len;
        Retrieve(len);
    }
    return len;
}