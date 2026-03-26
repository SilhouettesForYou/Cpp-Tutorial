// 01_minimal_coroutine.cpp
// 最小协程骨架示例：演示 co_await std::suspend_always，手动 resume 协程
// 编译: g++ -std=c++20 01_minimal_coroutine.cpp -o 01_minimal_coroutine

#include <coroutine>
#include <iostream>

struct MyCoroutine {
    struct promise_type {
        MyCoroutine get_return_object() {
            return MyCoroutine{
                std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }
        std::suspend_always initial_suspend() { return {}; } // 创建后先挂起
        std::suspend_always final_suspend() noexcept { return {}; } // 结束后保持挂起
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> handle;

    explicit MyCoroutine(std::coroutine_handle<promise_type> h) : handle(h) {}

    // RAII: 析构时销毁协程帧，防止内存泄漏
    ~MyCoroutine() {
        if (handle) handle.destroy();
    }

    void resume() { handle.resume(); }
    bool done() const { return handle.done(); }
};

MyCoroutine hello() {
    std::cout << "Step 1\n";
    co_await std::suspend_always{};  // 暂停，等待外部 resume()
    std::cout << "Step 2\n";
    co_await std::suspend_always{};  // 再次暂停
    std::cout << "Step 3\n";
    // 函数结束，协程完成
}

int main() {
    auto coro = hello();             // 协程创建但不立即执行（initial_suspend = always）
    std::cout << "Created coroutine\n";
    coro.resume();                   // 执行到第一个 co_await → 输出 Step 1
    coro.resume();                   // 执行到第二个 co_await → 输出 Step 2
    coro.resume();                   // 执行到末尾 → 输出 Step 3
    std::cout << "Done: " << std::boolalpha << coro.done() << "\n";
    return 0;
}
