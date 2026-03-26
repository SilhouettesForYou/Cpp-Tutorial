// ============================================================
// 示例 7: continuation - 协程链接（then 模式）
// 类似于 Promise.then()，用于异步操作流水线
// 适用于复杂异步工作流、链式调用
// ============================================================

#include <coroutine>
#include <iostream>
#include <functional>
#include <optional>

template <typename T>
struct continuation {
    struct promise_type {
        std::optional<T> value;
        std::function<void()> continuation_fn;
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { 
            if (continuation_fn) continuation_fn();
            return {}; 
        }
        continuation get_return_object() { return continuation{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        void unhandled_exception() {}
        template <typename V> void return_value(V&& v) { value = std::forward<V>(v); }
    };

    std::coroutine_handle<promise_type> h;
    continuation(std::coroutine_handle<promise_type> h) : h(h) {}
    ~continuation() { if (h) h.destroy(); }

    // then: 在当前协程完成后执行下一个协程
    template <typename F>
    auto then(F&& func) {
        using R = std::invoke_result_t<F, T>;
        struct wrapper {
            promise_type* parent;
            F func;
            struct awaitable {
                promise_type* parent;
                F& func;
                bool await_ready() const { return parent->value.has_value(); }
                void await_suspend(std::coroutine_handle<>) {}
                auto await_resume() { return func(std::move(parent->value.value())); }
            };
            auto operator co_await() { return awaitable{parent, func}; }
        };
        h.promise().continuation_fn = [this]() { h.resume(); };
        return wrapper{h.promise(), std::forward<F>(func)};
    }
};

continuation<int> step1() {
    std::cout << "[Step1] 计算初始值\n";
    co_return 10;
}

continuation<int> step2(int x) {
    std::cout << "[Step2] 乘以 2: " << x << " -> " << x * 2 << "\n";
    co_return x * 2;
}

continuation<std::string> step3(int x) {
    std::cout << "[Step3] 转换为字符串: " << x << "\n";
    co_return "结果: " + std::to_string(x);
}

int main() {
    std::cout << "[Main] 启动链式调用\n";
    auto result = step1()
        .then(step2)
        .then(step3);
    std::cout << "[Main] 最终结果: ";
    // 实际应用中会在事件循环中运行
    result.h.resume();
    std::cout << result.h.promise().value.value() << "\n";
    return 0;
}
