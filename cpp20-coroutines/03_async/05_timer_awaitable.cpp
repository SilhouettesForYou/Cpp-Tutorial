// ============================================================
// 示例 5: timer_awaitable - 定时器 Awaitable
// 实现 co_await 延时，支持协程内精确时间控制
// 适用于定时任务、动画、超时控制
// ============================================================

#include <coroutine>
#include <iostream>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

// 定时器任务
struct timer_awaitable {
    std::chrono::milliseconds duration;
    bool ready = false;

    bool await_ready() const { 
        std::this_thread::sleep_for(duration);
        return true;  // 同步等待完成
    }
    void await_suspend(std::coroutine_handle<>) {}
    void await_resume() const { std::cout << "[Timer] 时间到!\n"; }
};

// 异步定时器事件循环
struct async_timer {
    struct node {
        std::chrono::steady_clock::time_point when;
        std::coroutine_handle<> handle;
        bool operator<(const node& other) const { return when > other.when; } // 小顶堆
    };

    std::priority_queue<node> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    std::thread worker;

    async_timer() : worker([this] { run(); }) {}
    ~async_timer() { 
        { std::lock_guard<std::mutex> lock(mtx); }
        worker.join(); 
    }

    void run() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
            if (tasks.empty()) {
                // 无任务，睡眠等待
            } else {
                auto now = std::chrono::steady_clock::now();
                if (tasks.top().when <= now) {
                    auto task = tasks.top().handle;
                    tasks.pop();
                    lock.unlock();
                    task.resume();
                } else {
                    cv.wait_until(lock, tasks.top().when);
                }
            }
        }
    }

    void schedule_after(std::chrono::milliseconds ms, std::coroutine_handle<> h) {
        auto when = std::chrono::steady_clock::now() + ms;
        {
            std::lock_guard<std::mutex> lock(mtx);
            tasks.push({when, h});
        }
        cv.notify_one();
    }
};

// 可挂起的定时器 Awaitable
struct sleep_awaitable {
    std::chrono::milliseconds duration;
    async_timer& timer;

    sleep_awaitable(std::chrono::milliseconds d, async_timer& t) : duration(d), timer(t) {}

    bool await_ready() const { return false; }
    void await_suspend(std::coroutine_handle<> h) { timer.schedule_after(duration, h); }
    void await_resume() const {}
};

template <typename T>
struct async_task {
    struct promise_type {
        std::optional<T> value;
        std::exception_ptr exception;
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        async_task get_return_object() { return async_task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        void unhandled_exception() { exception = std::current_exception(); }
        template <typename V> void return_value(V&& v) { value = std::forward<V>(v); }
    };

    std::coroutine_handle<promise_type> h;
    async_task(std::coroutine_handle<promise_type> h) : h(h) {}
    ~async_task() { if (h) h.destroy(); }
};

// 带超时的协程
async_task<void> periodic_task(async_timer& timer, int id) {
    for (int i = 0; i < 3; ++i) {
        std::cout << "[Task " << id << "] 第 " << i + 1 << " 次执行\n";
        co_await sleep_awaitable{std::chrono::milliseconds(100), timer};
    }
    std::cout << "[Task " << id << "] 完成!\n";
}

int main() {
    async_timer timer;
    std::cout << "[Main] 启动定时任务\n";
    periodic_task(timer, 1);
    periodic_task(timer, 2);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "[Main] 所有任务完成\n";
    return 0;
}
