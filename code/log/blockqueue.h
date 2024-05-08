# ifndef WAN_BLOCKQUEUE_H
# define WAN_BLOCKQUEUE_H

#include <deque>
#include <condition_variable>
#include <mutex>
#include <sys/time.h>


template<typename T>
class BlockQueue {
public:
    explicit BlockQueue(int max_size = 1000) : capacity_(max_size) {}
    ~BlockQueue() { close(); }
    void close() {
        std::lock_guard<std::mutex> locker(mtx_);
        deque_.clear();
        is_close_ = true;
        condConsumer_.notify_all();
        condProducer_.notify_all();
    }
    void clear() {
        std::lock_guard<std::mutex> locker(mtx_);
        deque_.clear();
        condProducer_.notify_all();
        deque_.clear();
    }
    bool empty() {
        std::lock_guard<std::mutex> locker(mtx_);
        return deque_.empty();
    }
    bool full() {
        std::lock_guard<std::mutex> locker(mtx_);
        return deque_.size() >= capacity_;
    }
    void flush() {
        std::lock_guard<std::mutex> locker(mtx_);
        condConsumer_.notify_one();
    }
    void push(const T &item) {
        std::unique_lock<std::mutex> locker(mtx_);
        while(true) {
            if(deque_.size() >= capacity_) {
                condProducer_.wait(locker);
            }
            else if(is_close_) break;
            else {
                deque_.push_back(item);
                condConsumer_.notify_one();  
                break;
            }
        }
    }
    bool pop(T &str) {
        std::unique_lock<std::mutex> locker(mtx_);
        while(true) {
            if(deque_.size() <= 0) {
                condConsumer_.wait(locker);
            }
            else if(is_close_) return false;
            else {
                str = deque_.front();
                deque_.pop_front();
                condProducer_.notify_one();
                return true;
            }
        }
    }
    
private:
    int capacity_;
    bool is_close_;
    std::deque<T> deque_;
    std::mutex mtx_;
    std::condition_variable condConsumer_;   // 消费者条件变量
    std::condition_variable condProducer_;   // 生产者条件变量
};


# endif