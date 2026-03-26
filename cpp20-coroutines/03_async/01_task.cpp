// ============================================================
// 示例 1: Task<T> - 最简异步任务
// 适用于异步 I/O、延迟执行、轻量并发
// ============================================================

#include <coroutine>
#include <iostream>
#include <future>

template <typename T>
struct Task {
    struct promise_type {
        T value;
        std::exception_ptr exception;
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        Task get_return_object() { return Task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        void unhandled_exception() { exception = std::current_exception(); }
        template <typename V> void return_value(V&& v) { value = std::forward<V>(v); }
    };

    std::coroutine_handle<promise_type> h;
    Task(std::coroutine_handle<promise_type> h) : h(h) {}
    ~Task() { if (h) h.destroy(); }

    T get() {
        if (h.promise().exception) std::rethrow_exception(h.promise().exception);
        return std::move(h.promise().value);
    }

    bool is_done() const { return h.done(); }

    auto operator co_await() const {
        struct Awaiter {
            std::coroutine_handle<promise_type> h;
            bool await_ready() const { return h.done(); }
            void await_suspend(std::coroutine_handle<>) { /* 继续调度 */ }
            T await_resume() { return std::move(h.promise().value); }
        };
        return Awaiter{h};
    }
};

// 模拟异步读取
Task<int> async_read() {
    std::cout << "[Task] 开始异步读取...\n";
    co_await std::suspend_always{};  // 模拟异步等待
    std::cout << "[Task] 读取完成\n";
    co_return 42;
}

Task<void> demo_task() {
    std::cout << "[Demo] 开始\n";
    int result = co_await async_read();
    std::cout << "[Demo] 结果: " << result << "\n";
    std::cout << "[Demo] 结束\n";
}

int main() {
    std::cout << "[Main] 创建协程\n";
    auto task = demo_task();
    std::cout << "[Main] 协程已创建，等待完成...\n";
    // 在实际应用中，这里会进入事件循环
    while (!task.is_done()) {
        std::cout << "[Main] 事件循环运行中...\n";
        task.h.resume();
    }
    std::cout << "[Main] 完成\n";
    return 0;
}
