# ifndef WAN_THREADPOOL_H
# define WAN_THREADPOOL_H

# include <queue>
# include <mutex>
# include <condition_variable>
# include <functional>
# include <thread>
# include <assert.h>

class ThreadPool {
public:
    ThreadPool() = default;
    // make_shared可以将控制块和对象一起分配在堆上，从而避免了两次内存分配
    ThreadPool(int thread_number = 8, int max_request = 10000)
    : pool_m_(std::make_shared<PoolMember>()), thread_number_(thread_number), max_requests_(max_request){
        assert(thread_number_ > 0);
        for(int i = 0; i < thread_number_; i++) {
            std::thread([this]{
                std::unique_lock<std::mutex> locker(pool_m_ -> mtx_);
                while(true) {
                    if(!pool_m_ -> tasks_.empty()) {
                        auto task = std::move(pool_m_ -> tasks_.front());
                        pool_m_ -> tasks_.pop();
                        locker.unlock();
                        task();
                        locker.lock();
                    }
                    else if(pool_m_ -> isColsed) break;
                    else {
                        pool_m_ -> cond_.wait(locker);
                    }
                }
            }
            ).detach();
        }
    }

    void DestoryThPool() {
        if(pool_m_) pool_m_ -> isColsed = true;
        pool_m_ -> cond_.notify_all(); // 唤醒所有wait，使线程都跳出循环
    }
    ~ThreadPool() {
        DestoryThPool();
    }

    template<typename T>
    bool PushTask(T &&task) {
        std::lock_guard<std::mutex> locker(pool_m_ -> mtx_); // 析构时解锁
        if(pool_m_ -> tasks_.size() >= max_requests_) {
            return false;
        }
        pool_m_ -> tasks_.emplace(std::forward<T>(task)); // 完美转发
        pool_m_ -> cond_.notify_one(); // 唤醒一个wait
        return true;
    }

private:
    struct PoolMember {
        std::mutex mtx_;
        std::condition_variable cond_;
        std::queue<std::function<void()>> tasks_;
        bool isColsed;
    };
    std::shared_ptr<PoolMember> pool_m_;
    int thread_number_;        //线程池中的线程数
    int max_requests_;         //请求队列中允许的最大请求数
};



# endif