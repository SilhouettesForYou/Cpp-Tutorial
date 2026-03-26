// ============================================================
// 示例 6: async_read_write - 模拟异步 I/O
// 协程与异步 I/O 模型的结合
// 适用于网络编程、文件 I/O、数据库操作
// ============================================================

#include <coroutine>
#include <iostream>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include <optional>

// 模拟异步 I/O 操作状态
struct io_operation {
    std::chrono::steady_clock::time_point start_time;
    int duration_ms;
    std::coroutine_handle<> continuation;
    bool completed = false;
};

// 模拟 I/O 调度器
struct io_scheduler {
    std::queue<io_operation> pending;
    std::mutex mtx;
    std::thread worker;

    io_scheduler() : worker([this] { process_io(); }) {}
    ~io_scheduler() { worker.join(); }

    void process_io() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::lock_guard<std::mutex> lock(mtx);
            if (!pending.empty() && pending.front().completed) {
                auto op = pending.front();
                pending.pop();
                op.continuation.resume();
            }
        }
    }

    void submit(io_operation op) {
        std::lock_guard<std::mutex> lock(mtx);
        pending.push(op);
    }

    void complete_front() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!pending.empty()) pending.front().completed = true;
    }
};

// 异步读取 Awaitable
struct async_read_awaitable {
    io_scheduler& io;
    int bytes_read = 0;
    bool await_ready() const { return false; }
    void await_suspend(std::coroutine_handle<> h) {
        io.submit({std::chrono::steady_clock::now(), 50, h, false});
    }
    int await_resume() { return 1024; } // 返回读取的字节数
};

// 异步写入 Awaitable  
struct async_write_awaitable {
    io_scheduler& io;
    const char* data;
    bool await_ready() const { return false; }
    void await_suspend(std::coroutine_handle<> h) {
        io.submit({std::chrono::steady_clock::now(), 30, h, false});
    }
    bool await_resume() { return true; } // 返回写入是否成功
};

template <typename T>
struct io_task {
    struct promise_type {
        std::optional<T> value;
        std::exception_ptr exception;
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        io_task get_return_object() { return io_task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        void unhandled_exception() { exception = std::current_exception(); }
        template <typename V> void return_value(V&& v) { value = std::forward<V>(v); }
    };

    std::coroutine_handle<promise_type> h;
    io_task(std::coroutine_handle<promise_type> h) : h(h) {}
    ~io_task() { if (h) h.destroy(); }
};

// 模拟文件读写协程
io_task<size_t> async_file_copy(io_scheduler& io, const char* src, const char* dst) {
    std::cout << "[IO] 打开源文件: " << src << "\n";
    int bytes_read = co_await async_read_awaitable{io};
    std::cout << "[IO] 读取完成: " << bytes_read << " 字节\n";

    std::cout << "[IO] 打开目标文件: " << dst << "\n";
    bool write_ok = co_await async_write_awaitable{io, "data"};
    std::cout << "[IO] 写入完成: " << (write_ok ? "成功" : "失败") << "\n";

    co_return bytes_read;
}

io_task<void> async_http_request(io_scheduler& io) {
    std::cout << "[HTTP] 发送请求...\n";
    co_await async_read_awaitable{io}; // 模拟网络读取
    std::cout << "[HTTP] 收到响应\n";
    co_return;
}

int main() {
    io_scheduler io;
    std::cout << "[Main] 模拟异步 I/O 操作\n";
    async_file_copy(io, "input.txt", "output.txt");
    async_http_request(io);
    io.complete_front(); // 模拟 I/O 完成回调
    io.complete_front();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::cout << "[Main] I/O 操作完成\n";
    return 0;
}
