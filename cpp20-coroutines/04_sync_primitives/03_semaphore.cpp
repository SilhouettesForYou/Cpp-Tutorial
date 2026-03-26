// ============================================================
// C++20 Coroutines - Semaphore & Barrier
// 信号量 & 屏障：协程同步原语
// ============================================================

#include <coroutine>
#include <mutex>
#include <queue>
#include <atomic>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

// ===============================================================
// 信号量 (Semaphore)
// ============================================================
// 信号量是一种计数器，用于控制对共享资源的访问
// acquire() 减少计数，release() 增加计数

class AsyncSemaphore {
public:
    // 获取信号量
    struct AcquireOperation {
        AsyncSemaphore& sem;
        
        bool await_ready() {
            return sem.count_ > 0;
        }
        
        void await_suspend(std::coroutine_handle<> h) {
            sem.waiters_.push(h);
        }
        
        void await_resume() {
            --sem.count_;
        }
    };

    // 尝试获取（不阻塞）
    struct TryAcquireOperation {
        AsyncSemaphore& sem;
        
        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<>) {}
        bool await_resume() { 
            return sem.try_acquire(); 
        }
    };

    explicit AsyncSemaphore(int count) : count_(count) {}
    
    AcquireOperation acquire() { return {*this}; }
    TryAcquireOperation try_acquire() { return {*this}; }
    
    // 释放信号量
    void release() {
        std::coroutine_handle<> h;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!waiters_.empty()) {
                h = waiters_.front();
                waiters_.pop();
            } else {
                ++count_;
                return;
            }
        }
        // 恢复等待的协程，但不增加计数（直接转移）
        h.resume();
    }

private:
    bool try_acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ > 0) {
            --count_;
            return true;
        }
        return false;
    }
    
    int count_;
    std::mutex mutex_;
    std::queue<std::coroutine_handle<>> waiters_;
};

// ============================================================
// 屏障 (Barrier)
// ============================================================
// 屏障让 N 个协程等待，直到所有协程都到达屏障点后再一起继续执行

class AsyncBarrier {
public:
    struct WaitOperation {
        AsyncBarrier& barrier;
        size_t& generation;
        
        bool await_ready() { 
            std::lock_guard<std::mutex> lock(barrier.mutex_);
            return barrier.generation_ != generation;
        }
        
        void await_suspend(std::coroutine_handle<> h) {
            barrier.waiters_.push(h);
            ++barrier.wait_count_;
            
            if (barrier.wait_count_ == barrier.total_) {
                barrier.wait_count_ = 0;
                ++barrier.generation_;
                
                // 唤醒所有等待的协程
                while (!barrier.waiters_.empty()) {
                    barrier.waiters_.front().resume();
                    barrier.waiters_.pop();
                }
            }
        }
        
        void await_resume() {}
    };

    explicit AsyncBarrier(size_t count) : total_(count) {}
    
    WaitOperation wait() { 
        return {*this, generation_}; 
    }

private:
    friend struct WaitOperation;
    
    size_t total_;
    size_t wait_count_ = 0;
    size_t generation_ = 0;
    std::mutex mutex_;
    std::queue<std::coroutine_handle<>> waiters_;
};

// ============================================================
// 带超时的信号量
// ============================================================
class AsyncTimedSemaphore {
public:
    struct AcquireOperation {
        AsyncTimedSemaphore& sem;
        std::chrono::milliseconds timeout;
        
        bool await_ready() {
            return sem.try_acquire();
        }
        
        void await_suspend(std::coroutine_handle<> h) {
            sem.add_waiter(h, timeout);
        }
        
        void await_resume() {}
        
        struct TimeoutResult {
            bool acquired;
        };
    };

    explicit AsyncTimedSemaphore(int count) : count_(count) {}
    
    AcquireOperation acquire(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) {
        return {*this, timeout};
    }
    
    bool try_acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ > 0) {
            --count_;
            return true;
        }
        return false;
    }
    
    void release() {
        std::coroutine_handle<> h;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!waiters_.empty()) {
                h = waiters_.front().first;
                waiters_.pop();
            } else {
                ++count_;
                return;
            }
        }
        h.resume();
    }

private:
    void add_waiter(std::coroutine_handle<> h, std::chrono::milliseconds timeout) {
        std::lock_guard<std::mutex> lock(mutex_);
        waiters_.push({h, timeout});
    }
    
    int count_;
    std::mutex mutex_;
    std::queue<std::pair<std::coroutine_handle<>, std::chrono::milliseconds>> waiters_;
};

// ============================================================
// 演示
// ============================================================

struct DemoTask {
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<>) {}
    void await_resume() {}
};

// 信号量演示：限制并发数
DemoTask semaphore_worker(AsyncSemaphore& sem, int id) {
    std::cout << "[Worker " << id << "] 等待获取信号量...\n";
    
    co_await sem.acquire();
    
    std::cout << "[Worker " << id << "] 获取到信号量，开始工作\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "[Worker " << id << "] 工作完成，释放信号量\n";
    
    sem.release();
}

// 屏障演示：同步点
DemoTask barrier_worker(AsyncBarrier& barrier, int id) {
    std::cout << "[Worker " << id << "] 到达同步点 1...\n";
    co_await barrier.wait();
    std::cout << "[Worker " << id << "] 跨过同步点 1!\n";
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    std::cout << "[Worker " << id << "] 到达同步点 2...\n";
    co_await barrier.wait();
    std::cout << "[Worker " << id << "] 跨过同步点 2!\n";
}

int main() {
    std::cout << "=== Semaphore Demo ===\n\n";
    
    // 信号量限制最多 2 个并发
    AsyncSemaphore sem(2);
    
    std::cout << "启动 5 个工作协程，信号量限制为 2...\n\n";
    
    for (int i = 1; i <= 5; ++i) {
        semaphore_worker(sem, i);
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "\n\n=== Barrier Demo ===\n\n";
    
    // 屏障让 4 个协程同步
    AsyncBarrier barrier(4);
    
    std::cout << "启动 4 个协程，它们将在两个同步点同步...\n\n";
    
    for (int i = 1; i <= 4; ++i) {
        barrier_worker(barrier, i);
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    std::cout << "\n=== 同步原语演示完成 ===\n";
    
    return 0;
}
