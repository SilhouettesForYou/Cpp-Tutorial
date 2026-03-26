// ============================================================
// 示例 3: sync_wait - 同步等待异步任务
// 将协程桥接到同步代码，阻塞直到完成
// 适用于从同步函数调用异步协程
// ============================================================

#include <coroutine>
#include <iostream>
#include <thread>
#include <chrono>
#include <optional>

template <typename T>
struct sync_task {
    struct promise_type {
        std::optional<T> value;
        std::exception_ptr exception;
        std::coroutine_handle<> continuation;
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { 
            if (continuation) continuation.resume();
            return {}; 
        }
        sync_task get_return_object() { return sync_task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        void unhandled_exception() { exception = std::current_exception(); }
        template <typename V> void return_value(V&& v) { value = std::forward<V>(v); }
    };

    std::coroutine_handle<promise_type> h;
    sync_task(std::coroutine_handle<promise_type> h) : h(h) {}
    ~sync_task() { if (h) h.destroy(); }

    auto operator co_await() const {
        struct Awaiter {
            std::coroutine_handle<promise_type> h;
            bool await_ready() const { return h.done(); }
            void await_suspend(std::coroutine_handle<> continuation) { h.promise().continuation = continuation; }
            T await_resume() { return std::move(h.promise().value.value()); }
        };
        return Awaiter{h};
    }
};

template <typename T>
T sync_wait(sync_task<T> task) {
    std::thread worker{[&task]() { task.h.resume(); }};
    worker.join();
    if (task.h.promise().exception) std::rethrow_exception(task.h.promise().exception);
    return std::move(task.h.promise().value.value());
}

// 异步模拟
sync_task<int> async_multiply(int a, int b) {
    std::cout << "[Async] 开始计算 " << a << " * " << b << "\n";
    co_await std::suspend_always{};
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    co_return a * b;
}

int main() {
    std::cout << "[Main] 同步等待异步任务...\n";
    int result = sync_wait(async_multiply(6, 7));
    std::cout << "[Main] 结果: " << result << "\n";
    return 0;
}
