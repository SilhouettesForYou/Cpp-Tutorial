// 03_custom_awaitable.cpp
// 自定义 Awaitable：实现 await_ready / await_suspend / await_resume 三件套
// 模拟一个"需要外部调度器触发恢复"的异步操作
// 编译: g++ -std=c++20 03_custom_awaitable.cpp -o 03_custom_awaitable

#include <coroutine>
#include <functional>
#include <iostream>

// ── 自定义 Awaitable ─────────────────────────────────────────────
struct Delay {
    std::function<void()> callback; // 外部调度器持有此回调来恢复协程

    // 返回 false → 总是挂起（如果已经完成可返回 true 跳过挂起）
    bool await_ready() { return false; }

    // 协程挂起时调用：保存 resume 句柄，供调度器稍后调用
    void await_suspend(std::coroutine_handle<> h) {
        callback = [h]() mutable { h.resume(); };
        std::cout << "[调度器] 协程已挂起，等待外部触发...\n";
    }

    // 协程恢复后立即调用，返回值就是 co_await 表达式的值
    void await_resume() {
        std::cout << "[调度器] 协程已恢复\n";
    }
};

// ── 简单 Task（立即开始，不持有结果）────────────────────────────
struct Task {
    struct promise_type {
        Task get_return_object() { return {}; }
        std::suspend_never  initial_suspend() { return {}; }
        std::suspend_never  final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
};

// 全局 Delay 对象，供 main() 中的"调度器"访问
Delay delay_obj;

Task async_work() {
    std::cout << "【协程】开始执行\n";
    co_await delay_obj;              // 挂起，把控制权还给调用者
    std::cout << "【协程】恢复后继续执行，任务完成\n";
}

int main() {
    async_work();                    // 协程执行到 co_await 处挂起
    std::cout << "【主线程】协程挂起后，主线程继续...\n";
    std::cout << "【主线程】模拟调度器触发恢复\n";
    delay_obj.callback();            // 触发协程恢复
    std::cout << "【主线程】全部完成\n";
    return 0;
}
