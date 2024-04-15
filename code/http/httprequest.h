# ifndef WAN_HTTP_REQUEST_H
# define WAN_HTTP_REQUEST_H

# include <sys/stat.h>    // stat
# include <unordered_map>
# include <unordered_set>
# include <string>
# include <regex>    // 正则表达式
# include <errno.h>    
# include <mysql/mysql.h>  //mysql

# include "../log/log.h"
# include "../buffer/buffer.h"
# include "../pool/sqlconnpool.h"


enum HTTP_CODE
{
    NO_REQUEST,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION
};

class HttpRequest {
public:
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE
    {
        CHECK_REQUEST_LINE = 0,
        CHECK_HEADER,
        CHECK_CONTENT,
        FINISH
    };
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };


    HttpRequest() { srcDir_ = method_ = path_ = version_= body_ = ""; }
    ~HttpRequest() = default;

    void Init(const std::string& srcDir);
    HTTP_CODE parse(Buffer &readBuff_);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

    bool IsKeepAlive() const;

private:

    LINE_STATUS ParseLine(Buffer &buffer, std::string &str);
    bool ParseRequestLine(const std::string& line);
    void ParseHeader(const std::string& line);         // 处理请求头
    void ParseBody(const std::string& line);           // 处理请求体

    void ParsePath();                                  // 处理请求路径
    HTTP_CODE ReParsePath();                           // 统一路径
    void ParsePost();                                  // 处理Post事件
    void ParseFromUrlencoded();                        // 从url种解析编码

    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);  // 用户验证


    std::string srcDir_;
    struct stat mmFileStat_;
    
    CHECK_STATE check_state_;
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> post_;

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
    static int ConverHex(char ch);  // 16进制转换为10进制
};

# endif