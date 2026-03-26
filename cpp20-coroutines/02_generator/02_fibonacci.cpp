// 02_fibonacci.cpp
// 无限斐波那契序列生成器：演示 co_yield 配合无限循环
// 依赖 Generator<T>（此文件内联定义，无需额外头文件）
// 编译: g++ -std=c++20 02_fibonacci.cpp -o 02_fibonacci

#include <coroutine>
#include <optional>
#include <iostream>

// ── Generator<T>（同 01_generator.cpp，内联复用）────────────────
template<typename T>
struct Generator {
    struct promise_type {
        std::optional<T> current_value;
        Generator get_return_object() {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(T value) {
            current_value = std::move(value);
            return {};
        }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
    using Handle = std::coroutine_handle<promise_type>;
    Handle handle;
    explicit Generator(Handle h) : handle(h) {}
    ~Generator() { if (handle) handle.destroy(); }
    Generator(const Generator&) = delete;
    Generator(Generator&& o) noexcept : handle(o.handle) { o.handle = nullptr; }
    struct Iterator {
        Handle handle;
        bool operator!=(std::default_sentinel_t) const { return handle && !handle.done(); }
        Iterator& operator++() { handle.resume(); return *this; }
        const T& operator*() const { return *handle.promise().current_value; }
    };
    Iterator begin() { handle.resume(); return Iterator{handle}; }
    std::default_sentinel_t end() { return {}; }
};

// ── 无限斐波那契序列 ─────────────────────────────────────────────
// 注意：协程体是无限循环，调用方负责在适当时候 break
Generator<long long> fibonacci() {
    long long a = 0, b = 1;
    while (true) {
        co_yield a;
        auto next = a + b;
        a = b;
        b = next;
    }
}

// ── 无限自然数序列 ───────────────────────────────────────────────
Generator<int> natural_numbers(int start = 0) {
    int n = start;
    while (true) {
        co_yield n++;
    }
}

int main() {
    // 只取前 10 个斐波那契数
    std::cout << "前10个斐波那契数: ";
    int count = 0;
    for (long long n : fibonacci()) {
        std::cout << n << " ";
        if (++count == 10) break;
    }
    std::cout << "\n";

    // 只取前 5 个从 1 开始的自然数
    std::cout << "从1开始的5个自然数: ";
    count = 0;
    for (int n : natural_numbers(1)) {
        std::cout << n << " ";
        if (++count == 5) break;
    }
    std::cout << "\n";

    return 0;
}
