#include <unistd.h>
# include "server/webserver.h"


int main() {
    WebServer server(
        1316, 0, 0, 0,                // 端口 timeoutMs 全ET 子线程读写处理
        "localhost", 3306, "root", "123456", "yourdb", // Mysql配置 
        12, 8,                            // 连接池数量 线程池数量
        true, true, 1, 1024);             // 日志开关 是否异步 日志等级 日志异步队列容量 
    server.EventLoop();
}
