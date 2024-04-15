# include "httpfile.h"

const std::unordered_map<std::string, std::string> HttpFile::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};


void HttpFile::Init(const std::string& srcDir, std::string& path) {
    if(file_stat_ptr_) { UnmapFile(); }
    path_ = path;
    srcDir_ = srcDir;
    mmFileStat_ = { 0 };
    
    stat((srcDir_ + path_).data(), &mmFileStat_);
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if(srcFd < 0) { 
        return; 
    }
    //将文件映射到内存提高文件的访问速度  MAP_PRIVATE 建立一个写入时拷贝的私有映射
    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mmRet == -1) {
        close(srcFd);
        return; 
    }
    close(srcFd);

    // 文件能正常打开，创建记录对象
    file_stat_ptr_ = new FileStat;
    file_stat_ptr_ -> file_address_ = (char*)mmRet;
    file_stat_ptr_ -> file_size_ = mmFileStat_.st_size;
    file_stat_ptr_ -> file_type_ = GetFileType();
}
std::string HttpFile::GetFileType() {
    std::string::size_type idx = path_.find_last_of('.');
    if(idx == std::string::npos) {   // 最大值 find函数在找不到指定值得情况下会返回string::npos
        return "text/plain";
    }
    std::string suffix = path_.substr(idx);
    if(SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}


void HttpFile::UnmapFile() {
    if(file_stat_ptr_) {
        munmap(file_stat_ptr_ -> file_address_, file_stat_ptr_ -> file_size_);
        delete file_stat_ptr_;
        file_stat_ptr_ = nullptr;
    }
}

HttpFile::FileStat* HttpFile::GetFileStatPtr() {
    return file_stat_ptr_;
}
    
