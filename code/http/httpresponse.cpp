# include "httpresponse.h"

using namespace std;

const unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 500, "Internal Error"}
};


void HttpResponse::MakeResponse(Buffer &buffer, struct iovec *iov, int &iov_cnt) {
    AddStatusLine(buffer);
    AddHeader(buffer);
    AddContent(buffer);
    iov[0].iov_len = buffer.ReadableBytes();
    iov[0].iov_base = buffer.Peek();
    iov_cnt = 1;
    if(file_stat_ptr_) {
        assert(file_stat_ptr_ -> file_address_);
        iov[1].iov_base = file_stat_ptr_ -> file_address_;
        iov[1].iov_len = file_stat_ptr_ -> file_size_;
        iov_cnt = 2;
    }
}

void HttpResponse::AddStatusLine(Buffer &buffer) {
    std::string status = "HTTP/1.1";
    if(CODE_STATUS.count(code_) != 1) {
        code_ = 400;
    }
    status = status + " " + to_string(code_) + " " + CODE_STATUS.find(code_) -> second + "\r\n";
    buffer.Append(status);
}

void HttpResponse::AddHeader(Buffer &buffer) {
    buffer.Append("Connection: ");
    if(is_keep_alive_) {
        buffer.Append("keep-alive\r\n");
        buffer.Append("keep-alive: max=6, timeout=120\r\n");
    } else{
        buffer.Append("close\r\n");
    }
    buffer.Append("Content-type: " + file_stat_ptr_ -> file_type_ + "\r\n");
}

void HttpResponse::AddContent(Buffer &buffer) {
    if(file_stat_ptr_ && file_stat_ptr_ -> file_size_ != 0) {
        buffer.Append("Content-length: " + to_string(file_stat_ptr_ -> file_size_) + "\r\n\r\n");
    }
    else {
        ErrorContent(buffer, "File Error: Can't open or Is empty!");
    }
        
}

void HttpResponse::ErrorContent(Buffer& buffer, std::string message) 
{
    std::string body;
    std::string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        status = "Bad Request";
    }
    body += to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buffer.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buffer.Append(body);
}


        