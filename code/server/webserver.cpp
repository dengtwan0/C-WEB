# include "webserver.h"


void WebServer::InitTrigMode(int trig_mode) {
    listen_event_ = EPOLLRDHUP;
    conn_event_ = EPOLLRDHUP | EPOLLONESHOT;
    switch (trig_mode) {
    case 0:
        listen_event_ |= EPOLLET;
        conn_event_ |= EPOLLET;
        break;
    case 1:
        listen_event_ |= EPOLLET;
        break;
    case 2:
        conn_event_ |= EPOLLET;
        break;
    case 3:
        break;
    default:
        listen_event_ |= EPOLLET;
        conn_event_ |= EPOLLET;
        break;
    }
    HttpConn::isEt_ = (conn_event_ & EPOLLET);
}

bool WebServer::EventListen() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    int ret = 0;
    // 端口复用
    int flag = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, (const void *)&flag, sizeof(flag));
    if(ret < 0) {
        LOG_ERROR("set socket setsockopt error !");
        close(listen_fd_);
        return false;
    }
    
    // 绑定
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port_);
    ret = bind(listen_fd_, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listen_fd_);
        return false;
    }
    // 监听
    ret = listen(listen_fd_, 8);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listen_fd_);
        return false;
    }
    
    // 放入epoller中管理 
    ret = epoller_ -> AddFd(listen_fd_, listen_event_ | EPOLLIN);
    if(ret < 0) {
        LOG_ERROR("Add listen error!");
        close(listen_fd_);
        return false;
    }
    LOG_INFO("Server port:%d", port_);
    return true;
}

void WebServer::EventLoop() {
    int time_ms = -1;
    if(!stop_server_) { LOG_INFO("========== Server start =========="); }
    while(!stop_server_) {
        if(timeout_ms_ > 0) {
            time_ms = timer_ -> GetNextTick();
        }
        int ret = epoller_ -> Wait(time_ms);
        for(int i = 0; i < ret; ++ i) {
            int cur_fd = epoller_ -> GetEventFd(i);
            uint32_t cur_events = epoller_ -> GetEvents(i);
            if(cur_fd == listen_fd_) {
                DealListen();
            }
            else if(cur_events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(clients_.count(cur_fd) > 0);
                CloseConn(&clients_[cur_fd]);
            }
            else if(cur_events & EPOLLIN) {
                assert(clients_.count(cur_fd) > 0);
                DealRead(&clients_[cur_fd]);
            }
            else if(cur_events & EPOLLOUT) {
                assert(clients_.count(cur_fd) > 0);
                DealWrite(&clients_[cur_fd]);
            }
            else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void WebServer::DealListen() {
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    do {
        int client_fd = accept(listen_fd_, (struct sockaddr *)&client_addr, &len);
        if(client_fd <= 0) {
            return;
        }
        else if(HttpConn::clientCnt_ >= MAX_FD) {
            SendError(client_fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return ;
        }
        AddClient(client_fd, &client_addr);
    } while(listen_event_ & EPOLLET);
}
void WebServer::SendError(int fd, const char* info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::AddClient(int fd, sockaddr_in *addr_ptr) {
    // 加入clients哈希列表并初始化httpconn对象
    clients_[fd].Init(fd, addr_ptr);
    // 定时器
    if(timeout_ms_ > 0) {
        timer_ -> Add(fd, timeout_ms_, std::bind(&WebServer::CloseConn, this, &clients_[fd]));
    }
    // 加入epoller管理
    epoller_ -> AddFd(fd, conn_event_ | EPOLLIN);
    LOG_INFO("Client[%d] in!", clients_[fd].GetFd());

}

void WebServer::CloseConn(HttpConn *client) {
    LOG_INFO("Client[%d] quit!", client -> GetFd());
    if(client -> is_init_) {
        epoller_ -> DelFd(client -> GetFd());
        client -> Close();
    }
}

void WebServer::DealRead(HttpConn *client) {
    assert(client);
    ExtentTime(client);
    if(actor_model_ == 0) { // 读写都在线程，主线程不阻塞
        thread_pool_ -> PushTask(std::bind(&WebServer::OnRead, this, client));
    }
    else if(actor_model_ == 1) { // 读在主线程，处理在线程
        int ret = -1;
        int readErrno = 0;
        ret = client -> Read(&readErrno);         // 读取客户端套接字的数据，读到httpconn的读缓存区
        if(ret <= 0 && readErrno != EAGAIN) {   // 读异常就关闭客户端
            CloseConn(client);
            return;
        }
        thread_pool_ -> PushTask(std::bind(&WebServer::OnProcess, this, client));
    }
    else if(actor_model_ == 2) { // 读写都在线程，主线程阻塞等待，同步
        // ReactorRead(client);
    }
}
void WebServer::OnRead(HttpConn *client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client -> Read(&readErrno);         // 读取客户端套接字的数据，读到httpconn的读缓存区
    if(ret <= 0 && readErrno != EAGAIN) {   // 读异常就关闭客户端
        CloseConn(client);
        return;
    }
    // 业务逻辑的处理（先读后处理）
    OnProcess(client);
}
// void WebServer::ReactorRead(HttpConn *client);

void WebServer::DealWrite(HttpConn *client) {
    assert(client);
    ExtentTime(client);
    if(actor_model_ == 0) { // 读写都在线程，主线程不阻塞
        thread_pool_ -> PushTask(std::bind(&WebServer::OnWrite, this, client));
    }
    else if(actor_model_ == 1) { // 读在主线程，处理在线程
        OnWrite(client);
    }
    else if(actor_model_ == 2) { // 读写都在线程，主线程阻塞等待，同步
        // ReactorWrite(client);
    }
}
void WebServer::OnWrite(HttpConn *client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client -> Write(&writeErrno);
    if(client -> ToWriteBytes() == 0) {
        /* 传输完成 */
        if(client->IsKeepAlive()) {
            client -> InitBuffer();
            epoller_ -> ModFd(client->GetFd(), conn_event_ | EPOLLIN); // 回归换成监测读事件
            return;
        }
    }
    else if(ret < 0) {
        if(writeErrno == EAGAIN) {  // 缓冲区满了 
            /* 继续传输 */
            epoller_ -> ModFd(client->GetFd(), conn_event_ | EPOLLOUT);
            return;
        }
    }
    CloseConn(client);
}
// void WebServer::ReactorrWrite(HttpConn *client);

void WebServer::OnProcess(HttpConn *client) {
    if(client -> ReadProcess()) {
        if(!client -> WriteProcess())
            CloseConn(client);
        epoller_ -> ModFd(client -> GetFd(), conn_event_ | EPOLLOUT);
    }
    else {
        epoller_ -> ModFd(client -> GetFd(), conn_event_ | EPOLLIN);
    }
}
// void WebServer::DealSignal(bool& timeout, bool& stop_server);
void WebServer::ExtentTime(HttpConn *client) {
    if(timeout_ms_ > 0) {
        timer_ -> Adjust(client -> GetFd(), timeout_ms_);
    }
}