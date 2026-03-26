// 02_suspend_types.cpp
// 演示 initial_suspend / final_suspend 的不同行为：
//   - suspend_never  → 不挂起（立即执行 / 立即销毁）
//   - suspend_always → 挂起（需要外部 resume / 保留协程帧）
// 编译: g++ -std=c++20 02_suspend_types.cpp -o 02_suspend_types

#include <coroutine>
#include <iostream>

// ── 辅助：立即执行协程（initial = never）──────────────────────────
struct EagerCoroutine {
    struct promise_type {
        EagerCoroutine get_return_object() {
            return EagerCoroutine{
                std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }
        std::suspend_never  initial_suspend() { return {}; } // 立即执行
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
    std::coroutine_handle<promise_type> handle;
    explicit EagerCoroutine(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~EagerCoroutine() { if (handle) handle.destroy(); }
};

// ── 辅助：惰性协程（initial = always）────────────────────────────
struct LazyCoroutine {
    struct promise_type {
        LazyCoroutine get_return_object() {
            return LazyCoroutine{
                std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }
        std::suspend_always initial_suspend() { return {}; } // 先挂起
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
    std::coroutine_handle<promise_type> handle;
    explicit LazyCoroutine(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~LazyCoroutine() { if (handle) handle.destroy(); }
    void resume() { if (!handle.done()) handle.resume(); }
};

EagerCoroutine eager_demo() {
    std::cout << "[Eager] 协程体立即执行\n";
    co_return;
}

LazyCoroutine lazy_demo() {
    std::cout << "[Lazy]  协程体被 resume() 后才执行\n";
    co_return;
}

int main() {
    std::cout << "--- Eager (suspend_never) ---\n";
    std::cout << "调用 eager_demo()...\n";
    auto e = eager_demo(); // 创建后立即执行协程体
    std::cout << "eager_demo() 返回\n\n";

    std::cout << "--- Lazy (suspend_always) ---\n";
    std::cout << "调用 lazy_demo()...\n";
    auto l = lazy_demo();  // 创建后挂起，协程体尚未执行
    std::cout << "lazy_demo() 返回（协程尚未执行）\n";
    l.resume();            // 现在才执行
    return 0;
}
