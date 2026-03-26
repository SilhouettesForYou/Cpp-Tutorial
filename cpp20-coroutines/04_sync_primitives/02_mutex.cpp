// ============================================================
// C++20 Coroutines - Async Mutex
// 异步互斥锁：保护共享资源的协程安全版本
// ============================================================

#include <coroutine>
#include <mutex>
#include <queue>
#include <optional>
#include <iostream>
#include <thread>
#include <chrono>

// ============================================================
// 方案1：基础版 AsyncMutex（不支持递归）
// ============================================================
class AsyncMutex {
public:
    // 锁的等待操作
    struct LockOperation {
        AsyncMutex& mutex_;
        
        bool await_ready() { 
            return mutex_.try_lock(); 
        }
        
        void await_suspend(std::coroutine_handle<> h) {
            mutex_.enqueue_waiter(h);
        }
        
        void await_resume() {
            // 此时已经持有锁
        }
    };

    LockOperation lock() { return {*this}; }
    
    bool try_lock() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (locked_) return false;
        locked_ = true;
        return true;
    }
    
    void unlock() {
        std::coroutine_handle<> next;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (waiters_.empty()) {
                locked_ = false;
                return;
            }
            next = waiters_.front();
            waiters_.pop();
        }
        next.resume();
    }

private:
    void enqueue_waiter(std::coroutine_handle<> h) {
        std::lock_guard<std::mutex> lock(mutex_);
        waiters_.push(h);
    }
    
    std::mutex mutex_;
    bool locked_ = false;
    std::queue<std::coroutine_handle<>> waiters_;
};

// RAII 锁守卫
class AsyncLockGuard {
public:
    explicit AsyncLockGuard(AsyncMutex& m) : mutex_(m), locked_(false) {}
    AsyncLockGuard(const AsyncLockGuard&) = delete;
    AsyncLockGuard& operator=(const AsyncLockGuard&) = delete;
    
    ~AsyncLockGuard() { if (locked_) mutex_.unlock(); }
    
    struct Awaitable {
        AsyncLockGuard* guard;
        
        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<> h) { 
            guard->coro_ = h;
        }
        void await_resume() { guard->locked_ = true; }
    };
    
    Awaitable acquire() { return {this}; }
    
private:
    friend struct Awaitable;
    AsyncMutex& mutex_;
    bool locked_;
    std::coroutine_handle<> coro_;
};

// 使用 co_await guard.acquire() 获取锁
// guard 析构时自动释放

// ============================================================
// 方案2：支持超时锁
// ============================================================
class AsyncTimedMutex {
public:
    struct LockOperation {
        AsyncTimedMutex& mutex;
        std::chrono::milliseconds timeout;
        
        bool await_ready() { 
            return mutex.try_lock(); 
        }
        
        void await_suspend(std::coroutine_handle<> h) {
            mutex.enqueue_waiter(h, timeout);
        }
        
        bool await_resume() { 
            return true; // 如果恢复执行，说明获得了锁
        }
    };

    struct TryLockOperation {
        AsyncTimedMutex& mutex;
        
        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<>) {}
        bool await_resume() { return mutex.try_lock(); }
    };

    LockOperation lock(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) {
        return {*this, timeout};
    }
    TryLockOperation try_lock() { return {*this}; }
    
    bool try_lock() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (locked_) return false;
        locked_ = true;
        return true;
    }
    
    void unlock() {
        std::coroutine_handle<> next;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (waiters_.empty()) {
                locked_ = false;
                return;
            }
            next = waiters_.front().first;
            waiters_.pop();
        }
        next.resume();
    }

private:
    void enqueue_waiter(std::coroutine_handle<> h, std::chrono::milliseconds timeout) {
        std::lock_guard<std::mutex> lock(mutex_);
        waiters_.push({h, timeout});
    }
    
    std::mutex mutex_;
    bool locked_ = false;
    std::queue<std::pair<std::coroutine_handle<>, std::chrono::milliseconds>> waiters_;
};

// ============================================================
// 读写锁示例
// ============================================================
class AsyncRwMutex {
public:
    struct ReadLock {
        AsyncRwMutex& mutex_;
        bool await_ready() { return mutex_.try_read_lock(); }
        void await_suspend(std::coroutine_handle<> h) { mutex_.enqueue_read_waiter(h); }
        void await_resume() {}
    };
    
    struct WriteLock {
        AsyncRwMutex& mutex_;
        bool await_ready() { return mutex_.try_write_lock(); }
        void await_suspend(std::coroutine_handle<> h) { mutex_.enqueue_write_waiter(h); }
        void await_resume() {}
    };

    ReadLock read_lock() { return {*this}; }
    WriteLock write_lock() { return {*this}; }
    
    void read_unlock() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (--readers_ == 0 && !write_waiters_.empty()) {
            auto h = write_waiters_.front();
            write_waiters_.pop();
            h.resume();
        }
    }
    
    void write_unlock() {
        std::coroutine_handle<> h;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!write_waiters_.empty()) {
                h = write_waiters_.front();
                write_waiters_.pop();
            } else {
                writers_ = false;
                // 唤醒所有等待的读者
                while (!read_waiters_.empty()) {
                    read_waiters_.front().resume();
                    read_waiters_.pop();
                }
            }
        }
        if (h) h.resume();
    }

private:
    bool try_read_lock() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (writers_) return false;
        readers_++;
        return true;
    }
    
    bool try_write_lock() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (writers_ || readers_ > 0) return false;
        writers_ = true;
        return true;
    }
    
    void enqueue_read_waiter(std::coroutine_handle<> h) {
        std::lock_guard<std::mutex> lock(mutex_);
        read_waiters_.push(h);
    }
    
    void enqueue_write_waiter(std::coroutine_handle<> h) {
        std::lock_guard<std::mutex> lock(mutex_);
        write_waiters_.push(h);
    }
    
    std::mutex mutex_;
    int readers_ = 0;
    bool writers_ = false;
    std::queue<std::coroutine_handle<>> read_waiters_;
    std::queue<std::coroutine_handle<>> write_waiters_;
};

// ============================================================
// 演示
// ============================================================

AsyncMutex shared_mutex;
int shared_value = 0;

struct WorkerTask {
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<>) {}
    void await_resume() {}
};

WorkerTask worker(int id, int iterations) {
    for (int i = 0; i < iterations; ++i) {
        // 使用互斥锁保护共享资源
        co_await shared_mutex.lock();
        int old = shared_value;
        std::cout << "[Worker " << id << "] 读值: " << old << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        shared_value = old + 1;
        std::cout << "[Worker " << id << "] 写值: " << shared_value << "\n";
        shared_mutex.unlock();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

int main() {
    std::cout << "=== Async Mutex Demo ===\n\n";
    
    // 启动多个工作协程，它们会竞争同一个互斥锁
    worker(1, 3);
    worker(2, 3);
    worker(3, 3);
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "\n最终值: " << shared_value << " (应该是 9)\n";
    std::cout << "\n=== 互斥锁演示完成 ===\n";
    
    return 0;
}
