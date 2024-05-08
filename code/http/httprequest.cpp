# include "httprequest.h"


// 网页名称
const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML {
    "/index", "/register", "/login", "/welcome", "/video", "/picture"
};

// 登录/注册
const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG {
    {"/login.html", 1}, {"/register.html", 0}
};


// 初始化操作，一些清零操作
void HttpRequest::Init(const std::string& srcDir) {
    srcDir_ = srcDir;
    mmFileStat_ = { 0 };
    check_state_ = CHECK_REQUEST_LINE;  // 初始状态
    method_ = path_ = version_= body_ = "";
    header_.clear();
    post_.clear();
}

HTTP_CODE HttpRequest::parse(Buffer &buffer) {
    HTTP_CODE ret = NO_REQUEST;
    LINE_STATUS lin_status = LINE_OK;

    while(buffer.ReadableBytes() && check_state_ != FINISH) {
        std::string line;
        lin_status = ParseLine(buffer, line);
        if(lin_status == LINE_BAD) {
            path_ = "/400.html";
            LOG_DEBUG("BAD_REQUEST:[%s]", line.c_str());
            return BAD_REQUEST;
        }    
        else if(lin_status == LINE_OPEN)  {
            LOG_DEBUG("NO_REQUEST");
            return NO_REQUEST;
        }

        // std::cout << "报文行:" << line << std::endl;
        // LINE_OK
        switch (check_state_)
        {
            case CHECK_REQUEST_LINE:
                // 解析错误
                if(!ParseRequestLine(line)) {
                    path_ = "/400.html";
                    return BAD_REQUEST;
                }
                ParsePath();
                break;
            case CHECK_HEADER:
                ParseHeader(line);
                if(buffer.ReadableBytes() <= 2) {  // 说明后面只有\r\n
                    check_state_ = FINISH;   // 提前结束
                }
                break;
            case CHECK_CONTENT: // CHECK_HEADER中会把空行读完
                ParseBody(line);
                break;
            default:
                break;
        }

    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return ReParsePath();   // 解析路径;
}

HttpRequest::LINE_STATUS HttpRequest::ParseLine(Buffer &buffer, std::string &str) {
    const char END[] = "\r\n";
    const char *begin = buffer.Peek(), *end = buffer.BeginWrite();
    // 读消息体
    if(check_state_ == CHECK_CONTENT) {
        assert(header_.count("Content-Length"));
        size_t len = static_cast<size_t>(std::stol(header_["Content-Length"].c_str()));
        // std::cout << "bufferlen:" << buffer.ReadableBytes() << " len:" << len << std::endl;
        if(buffer.ReadableBytes() >= len) {
            str = std::string(begin, begin + len);
            buffer.Retrieve(len);
            return LINE_OK;
        }
        return LINE_OPEN;
    }
    const char* it = std::search(begin, end, END, END+1);
    if(it != end && *it == '\r') {
        // 如果末尾只有一个\r则说明没接收完全
        if(it + 1 == end) return HttpRequest::LINE_OPEN;
        // 语法正确，提取该行
        else if(*(it + 1) == '\n') {
            str = std::string(begin, it);
            buffer.RetrieveUntil(it + 2); 
            return HttpRequest::LINE_OK;
        }
    }
    // 没有\r但有\n则语法错误
    it = std::search(begin, end, END+1, END+2);
    if(it != end && *it == '\n') {
        LOG_DEBUG("LINE_BAD:No r");
        return HttpRequest::LINE_BAD;
    }
    // \r\n都没有，接收不完全
    return HttpRequest::LINE_OPEN;
}

bool HttpRequest::ParseRequestLine(const std::string& line) {
    std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch Match;   // 用来匹配patten得到结果
    // 在匹配规则中，以括号()的方式来划分组别 一共三个括号 [0]表示整体
    if(std::regex_match(line, Match, patten)) {  // 匹配指定字符串整体是否符合
        method_ = Match[1];
        path_ = Match[2];
        version_ = Match[3];
        HttpRequest::check_state_ = CHECK_HEADER;
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

void HttpRequest::ParsePath() {
    if(path_ == "/") {
        path_ = "/index.html";
    } 
    else {
        if(DEFAULT_HTML.find(path_) != DEFAULT_HTML.end()) {
            path_ += ".html";
        }
    }
}

// 解析路径，统一一下path名称,方便后面解析资源
HTTP_CODE HttpRequest::ReParsePath() {
    if (stat((srcDir_ + path_).data(), &mmFileStat_) < 0) {
        path_ = "/404.html";
        return NO_RESOURCE;
    }

    //判断文件的权限，是否可读，不可读则返回FORBIDDEN_REQUEST状态
    if (!(mmFileStat_.st_mode & S_IROTH)) {
        path_ = "/403.html";
        return FORBIDDEN_REQUEST;
    }

    // 判断文件类型，如果是目录，则返回BAD_REQUEST，表示请求报文有误
    if (S_ISDIR(mmFileStat_.st_mode)) {
        path_ = "/400.html";
        return BAD_REQUEST;
    }
 
    return FILE_REQUEST;
}

void HttpRequest::ParseHeader(const std::string& line) {
    std::regex patten("^([^:]*): ?(.*)$");
    std::smatch Match;
    if(std::regex_match(line, Match, patten)) {
        header_[Match[1]] = Match[2];
    } else {    // 匹配失败说明首部行匹配完了，此时是空行，状态变化
        check_state_ = CHECK_CONTENT;
    }
}

void HttpRequest::ParseBody(const std::string& line) {
    body_ = line;
    ParsePost();
    check_state_ = FINISH;    // 状态转换为下一个状态
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}


// 16进制转化为10进制
int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') 
        return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') 
        return ch -'a' + 10;
    return ch;
}

// 处理post请求
void HttpRequest::ParsePost() {
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded();     // POST请求体示例
        if(DEFAULT_HTML_TAG.count(path_)) { // 如果是登录/注册的path
            int tag = DEFAULT_HTML_TAG.find(path_)->second; 
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);  // 为1则是登录
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";
                } 
                else {
                    path_ = "/error.html";
                }
            }
        }
    }   
}

// 从url中解析编码
void HttpRequest::ParseFromUrlencoded() {
    if(body_.size() == 0) { return; }

    std::string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for(; i < n; i++) {
        char ch = body_[i];
        switch (ch) {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            body_[i] = ' ';
            break;
        case '%':
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    if(post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

bool HttpRequest::UserVerify(const std::string &name, const std::string &pwd, bool isLogin) {
    if(name == "" || pwd == "") { return false; }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    SqlConnRAII(&sql, SqlConnPool::GetInstance());
    assert(sql);
    
    bool flag = false;
    unsigned int j = 0;
    char order[256] = { 0 };
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;
    
    if(!isLogin) { flag = true; }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if(mysql_query(sql, order)) { 
        mysql_free_result(res);
        return false; 
    }
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        std::string password(row[1]);
        if(isLogin) {
            if(pwd == password) { flag = true; }
            else {
                flag = false;
                LOG_INFO("pwd error!");
            }
        } 
        else { 
            flag = false; 
            LOG_INFO("user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if(!isLogin && flag == true) {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG( "%s", order);
        if(mysql_query(sql, order)) { 
            LOG_DEBUG( "Insert error!");
            flag = false; 
        }
        flag = true;
    }
    // SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG( "UserVerify success!!");
    return flag;
}

std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}