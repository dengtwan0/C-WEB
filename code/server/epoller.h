# ifndef WAN_EPOLLER_H
# define WAN_EPOLLER_H

# include<sys/epoll.h>
#include <unistd.h> // close()
#include <assert.h> // close()
#include <vector>
#include <errno.h>
#include <fcntl.h>       // fcntl()

class Epoller {
public:
    Epoller(int max_events = 1024)
    : max_events_(max_events), events_(max_events), epfd_(epoll_create(1024)) {
        assert(epfd_ >= 0 && events_.size() > 0);
    }
    ~Epoller() {
        close(epfd_);
    }

    bool AddFd(int fd, uint32_t events);
    bool ModFd(int fd, uint32_t events);
    bool DelFd(int fd);
    int Wait(int timeMs = -1);
    int GetEventFd(int i) const;
    uint32_t GetEvents(int i) const;

private:
    std::vector<struct epoll_event> events_;
    int max_events_;
    
    int epfd_;
};



# endif