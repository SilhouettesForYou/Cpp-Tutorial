// ============================================================
// 示例 2: lazy_task - 惰性执行的任务（不自动启动）
// 只有调用 .start() 或 co_await 时才执行
// 适用于需要手动控制执行时机的场景
// ============================================================

#include <coroutine>
#include <iostream>
#include <functional>

template <typename T>
struct lazy_task {
    struct promise_type {
        std::optional<T> value;
        std::exception_ptr exception;
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        lazy_task get_return_object() { return lazy_task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        void unhandled_exception() { exception = std::current_exception(); }
        template <typename V> void return_value(V&& v) { value = std::forward<V>(v); }
    };

    std::coroutine_handle<promise_type> h;
    bool started = false;

    lazy_task(std::coroutine_handle<promise_type> h) : h(h) {}
    ~lazy_task() { if (h) h.destroy(); }

    void start() {
        if (!started && h) {
            started = true;
            h.resume();
        }
    }

    T get() {
        if (h.promise().exception) std::rethrow_exception(h.promise().exception);
        return std::move(h.promise().value.value());
    }

    auto operator co_await() const {
        struct Awaiter {
            std::coroutine_handle<promise_type> h;
            bool await_ready() const { return h.done(); }
            void await_suspend(std::coroutine_handle<>) {}
            T await_resume() { return std::move(h.promise().value.value()); }
        };
        return Awaiter{h};
    }
};

// 惰性任务示例
lazy_task<int> lazy_compute(int x, int y) {
    std::cout << "[Lazy] 开始计算 " << x << " + " << y << "\n";
    co_await std::suspend_always{};
    std::cout << "[Lazy] 计算中...\n";
    co_return x + y;
}

lazy_task<std::string> lazy_fetch_data() {
    std::cout << "[Lazy] 准备获取数据\n";
    co_await std::suspend_always{};
    std::cout << "[Lazy] 获取数据中...\n";
    co_return "Hello, Coroutine!";
}

int main() {
    std::cout << "[Main] 创建惰性任务（未启动）\n";
    auto task1 = lazy_compute(10, 20);
    auto task2 = lazy_fetch_data();

    std::cout << "[Main] 任务已创建，执行到第一个 suspension point\n";
    std::cout << "[Main] 现在启动 task1...\n";
    task1.start();

    std::cout << "[Main] task1 结果: " << task1.get() << "\n";
    std::cout << "[Main] 现在启动 task2...\n";
    task2.start();
    std::cout << "[Main] task2 结果: " << task2.get() << "\n";

    return 0;
}
