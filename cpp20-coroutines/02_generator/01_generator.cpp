// 01_generator.cpp
// Generator<T>：用 co_yield 实现惰性序列生成器，支持范围 for 循环
// 示例：生成 [from, to) 整数区间
// 编译: g++ -std=c++20 01_generator.cpp -o 01_generator

#include <coroutine>
#include <optional>
#include <iostream>

// ── 通用 Generator<T> ────────────────────────────────────────────
template<typename T>
struct Generator {
    struct promise_type {
        std::optional<T> current_value;

        Generator get_return_object() {
            return Generator{
                std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        // co_yield value 调用此方法：保存值并挂起
        std::suspend_always yield_value(T value) {
            current_value = std::move(value);
            return {};
        }

        void return_void() { current_value.reset(); }
        void unhandled_exception() { std::terminate(); }
    };

    using Handle = std::coroutine_handle<promise_type>;
    Handle handle;

    explicit Generator(Handle h) : handle(h) {}
    ~Generator() { if (handle) handle.destroy(); }

    // 禁止拷贝，允许移动
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    Generator(Generator&& o) noexcept : handle(o.handle) { o.handle = nullptr; }

    // ── 迭代器 ──────────────────────────────────────────────────
    struct Iterator {
        Handle handle;

        // 与 sentinel 比较：协程完成则迭代结束
        bool operator!=(std::default_sentinel_t) const {
            return handle && !handle.done();
        }

        // 前进：恢复协程，让它产出下一个值
        Iterator& operator++() {
            handle.resume();
            return *this;
        }

        // 解引用：返回当前产出的值
        const T& operator*() const {
            return *handle.promise().current_value;
        }
    };

    Iterator begin() {
        handle.resume(); // 启动协程，执行到第一个 co_yield
        return Iterator{handle};
    }
    std::default_sentinel_t end() { return {}; }
};

// ── 示例协程：生成 [from, to) 整数 ───────────────────────────────
Generator<int> range(int from, int to) {
    for (int i = from; i < to; ++i) {
        co_yield i;
    }
}

// ── 示例协程：生成所有偶数（有限范围） ──────────────────────────
Generator<int> even_numbers(int max) {
    for (int i = 0; i <= max; i += 2) {
        co_yield i;
    }
}

int main() {
    std::cout << "range(1, 6): ";
    for (int x : range(1, 6)) {
        std::cout << x << " ";
    }
    std::cout << "\n";

    std::cout << "even_numbers(10): ";
    for (int x : even_numbers(10)) {
        std::cout << x << " ";
    }
    std::cout << "\n";

    return 0;
}
