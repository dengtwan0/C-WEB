# ifndef WAN_HTTP_FILE_H
# define WAN_HTTP_FILE_H

# include<unordered_map>
# include <fcntl.h>       // open
# include <unistd.h>      // close
# include <sys/stat.h>    // stat
# include <sys/mman.h>    // mmap, munmap

# include "../log/log.h"

class HttpFile {
public:
    HttpFile() : file_stat_ptr_(nullptr) {}
    ~HttpFile() {
        UnmapFile();
    }

    void Init(const std::string& srcDir, std::string& path);

    void UnmapFile();
    size_t FileLen() const { return mmFileStat_.st_size; }

    std::string GetFileType();

    typedef struct FileStat {
        size_t file_size_ = 0;
        std::string file_type_ = "text/plain";
        char *file_address_ = nullptr;
    }FileStat;

    FileStat* GetFileStatPtr();
    

private:
    FileStat *file_stat_ptr_;
    
    std::string path_;
    std::string srcDir_;

    struct stat mmFileStat_;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;  // 后缀类型集   
};


# endif