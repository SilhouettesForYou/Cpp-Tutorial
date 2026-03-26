// ============================================================
// C++20 Coroutines - Cancellation & Timeout
// 取消与超时：协程的取消机制和超时控制
// ============================================================

#include <coroutine>
#include <chrono>
#include <iostream>
#include <thread>
#include <atomic>
#include <optional>
#include <functional>

// ============================================================
// 取消令牌
// ============================================================
class CancellationToken {
public:
    CancellationToken() : cancelled_(false) {}
    CancellationToken(std::atomic<bool>* external) : cancelled_(external) {}
    
    bool is_cancelled() const {
        return cancelled_.load();
    }
    
    void cancel() {
        cancelled_.store(true);
    }
    
    void reset() {
        cancelled_.store(false);
    }

private:
    std::atomic<bool> cancelled_;
};

// ============================================================
// 可取消的任务
// ============================================================
template<typename T>
class CancellableTask {
public:
    struct promise_type {
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        CancellableTask get_return_object() {
            return CancellableTask(std::coroutine_handle<promise_type>::from_promise(*this));
        }
        
        void return_value(T value) {
            result_ = std::make_shared<T>(std::move(value));
        }
        
        void unhandled_exception() {
            exception_ = std::current_exception();
        }
        
        std::shared_ptr<T> result_;
        std::exception_ptr exception_;
    };
    
    CancellableTask(std::coroutine_handle<promise_type> h) : handle_(h) {}
    CancellableTask(CancellableTask&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    ~CancellableTask() { if (handle_) handle_.destroy(); }
    
    std::coroutine_handle<promise_type> handle() { return handle_; }
    
    T get() {
        if (handle_) {
            handle_.resume();
            if (handle_.promise().exception_) {
                std::rethrow_exception(handle_.promise().exception_);
            }
            return *handle_.promise().result_;
        }
        return T{};
    }

private:
    std::coroutine_handle<promise_type> handle_;
};

// ============================================================
// 取消等待操作
// ============================================================
template<typename T>
struct CancelAwareAwaiter {
    std::coroutine_handle<> continuation;
    T* result;
    CancellationToken* token;
    
    bool await_ready() {
        return false;
    }
    
    void await_suspend(std::coroutine_handle<> h) {
        continuation = h;
    }
    
    T await_resume() {
        return *result;
    }
};

// ============================================================
// 超时操作
// ============================================================
class TimeoutOperation {
public:
    TimeoutOperation(std::chrono::milliseconds timeout) 
        : timeout_(timeout), expired_(false) {}
    
    bool await_ready() {
        return false;
    }
    
    void await_suspend(std::coroutine_handle<> h) {
        timer_ = std::thread([this, h]() {
            std::this_thread::sleep_for(timeout_);
            if (!done_) {
                expired_ = true;
                h.resume();
            }
        });
    }
    
    bool await_resume() {
        done_ = true;
        if (timer_.joinable()) timer_.join();
        return !expired_;
    }

private:
    std::chrono::milliseconds timeout_;
    std::atomic<bool> done_{false};
    std::atomic<bool> expired_{false};
    std::thread timer_;
};

// ============================================================
// 限时等待任务
// ============================================================
template<typename T>
struct TimedWaitOperation {
    std::coroutine_handle<> continuation;
    std::shared_ptr<T> result;
    std::chrono::milliseconds timeout;
    bool success = false;
    
    bool await_ready() { return false; }
    
    void await_suspend(std::coroutine_handle<> h) {
        continuation = h;
        std::thread worker([this]() {
            std::this_thread::sleep_for(timeout_);
            if (!done_) {
                done_ = true;
                success = false;
                continuation.resume();
            }
        });
        // 模拟工作
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (!done_) {
            done_ = true;
            success = true;
            continuation.resume();
        }
    }
    
    bool await_resume() { return success; }

private:
    std::atomic<bool> done_{false};
};

// ============================================================
// 取消检查点
// ============================================================
struct CancellationPoint {
    CancellationToken token;
    
    struct Awaiter {
        CancellationToken* token;
        
        bool await_ready() { return token->is_cancelled(); }
        void await_suspend(std::coroutine_handle<>) {}
        void await_resume() {}
    };
    
    Awaiter wait() { return {&token}; }
};

// ============================================================
// 演示
// ============================================================
struct DemoTask {
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<>) {}
    void await_resume() {}
};

DemoTask cancellable_work(CancellationToken token, int id) {
    for (int i = 0; i < 10; ++i) {
        // 取消检查点
        co_await CancellationPoint{token}.wait();
        
        std::cout << "[Task " << id << "] Step " << i << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "[Task " << id << "] 完成\n";
}

DemoTask timed_work() {
    std::cout << "[TimedTask] 开始，等待 500ms 超时...\n";
    
    bool ok = co_await TimeoutOperation(std::chrono::milliseconds(500));
    
    if (ok) {
        std::cout << "[TimedTask] 超时前完成!\n";
    } else {
        std::cout << "[TimedTask] 超时了!\n";
    }
}

DemoTask timeout_wait_demo() {
    std::cout << "[TimeoutDemo] 尝试限时操作...\n";
    
    int result = 42;
    bool ok = co_await TimedWaitOperation<int>{.result = std::make_shared<int>(result), .timeout = std::chrono::milliseconds(100)};
    
    if (ok) {
        std::cout << "[TimeoutDemo] 操作成功，结果: " << result << "\n";
    } else {
        std::cout << "[TimeoutDemo] 操作超时!\n";
    }
}

int main() {
    std::cout << "=== Cancellation & Timeout Demo ===\n\n";
    
    // 取消演示
    std::cout << "--- Cancellation Demo ---\n";
    CancellationToken token;
    
    // 启动两个任务
    auto task1 = cancellable_work(token, 1);
    auto task2 = cancellable_work(token, 2);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    std::cout << "[Main] 取消所有任务!\n";
    token.cancel();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 超时演示
    std::cout << "\n--- Timeout Demo ---\n";
    timed_work();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    std::cout << "\n--- Timed Wait Demo ---\n";
    timeout_wait_demo();
    
    std::cout << "\n=== 取消与超时演示完成 ===\n";
    
    return 0;
}
