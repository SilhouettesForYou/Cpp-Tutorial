// ============================================================
// 示例 4: custom_scheduler - 自定义调度器
// 实现线程池调度，让协程在指定线程执行
// 适用于需要控制执行线程的场景
// ============================================================

#include <coroutine>
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

// 简单线程池调度器
struct thread_pool {
    std::queue<std::coroutine_handle<>> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop = false;
    std::vector<std::thread> workers;

    thread_pool(size_t threads = 2) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::coroutine_handle<> task;
                    {
                        std::unique_lock<std::mutex> lock(mtx);
                        cv.wait(lock, [this] { return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) return;
                        task = tasks.front();
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    ~thread_pool() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stop = true;
        }
        cv.notify_all();
        for (auto& w : workers) w.join();
    }

    void schedule(std::coroutine_handle<> task) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            tasks.push(task);
        }
        cv.notify_one();
    }

    int get_thread_id() {
        return static_cast<int>(std::hash<std::thread::id>{}(std::this_thread::get_id())) % 1000;
    }
};

// 自定义调度器 Awaitable
struct scheduler_awaitable {
    thread_pool& pool;
    bool await_ready() const { return false; }
    void await_suspend(std::coroutine_handle<> h) { pool.schedule(h); }
    void await_resume() const {}
};

template <typename T>
struct scheduled_task {
    struct promise_type {
        std::optional<T> value;
        std::exception_ptr exception;
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        scheduled_task get_return_object() { return scheduled_task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        void unhandled_exception() { exception = std::current_exception(); }
        template <typename V> void return_value(V&& v) { value = std::forward<V>(v); }
    };

    std::coroutine_handle<promise_type> h;
    scheduled_task(std::coroutine_handle<promise_type> h) : h(h) {}
    ~scheduled_task() { if (h) h.destroy(); }

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

// 使用线程池的协程
scheduled_task<int> pool_task(thread_pool& pool, int id) {
    std::cout << "[Task " << id << "] 开始执行，线程ID: " << pool.get_thread_id() << "\n";
    co_await scheduler_awaitable{pool};
    std::cout << "[Task " << id << "] 恢复执行，线程ID: " << pool.get_thread_id() << "\n";
    co_return id * 10;
}

int main() {
    thread_pool pool(2);
    std::cout << "[Main] 主线程ID: " << pool.get_thread_id() << "\n";
    std::cout << "[Main] 提交任务到线程池...\n";
    pool.schedule(pool_task(pool, 1).h);
    pool.schedule(pool_task(pool, 2).h);
    pool.schedule(pool_task(pool, 3).h);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "[Main] 所有任务完成\n";
    return 0;
}
