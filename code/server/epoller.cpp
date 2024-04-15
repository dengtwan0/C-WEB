# include "epoller.h"


bool Epoller::AddFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    if(epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) == 0) {
        // 设置非阻塞
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
        return true;
    }
    return false;
}

bool Epoller::ModFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::DelFd(int fd) {
    if(fd < 0) return false;
    return 0 == epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
}

int Epoller::Wait(int timeMs) {
    return epoll_wait(epfd_, &events_[0], static_cast<int>(events_.size()), timeMs);
}

int Epoller::GetEventFd(int i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].data.fd;
}
uint32_t Epoller::GetEvents(int i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].events;
}

