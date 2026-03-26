// ============================================================
// C++20 Coroutines - Generator Pipeline
// 生成器流水线：将多个生成器串联成数据处理管道
// ============================================================

#include <coroutine>
#include <vector>
#include <optional>
#include <functional>
#include <iostream>
#include <thread>
#include <chrono>

// ============================================================
// 基础生成器
// ============================================================
template<typename T>
class Generator {
public:
    struct promise_type {
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        Generator get_return_object() {
            return Generator(std::coroutine_handle<promise_type>::from_promise(*this));
        }
        
        std::suspend_always yield_value(T value) {
            current_value_ = value;
            return {};
        }
        
        void return_void() {}
        void unhandled_exception() { throw; }
        
        T current_value_;
    };
    
    struct iterator {
        std::coroutine_handle<promise_type> coro;
        bool done = false;
        
        iterator(std::coroutine_handle<promise_type> c, bool d = false) : coro(c), done(d) {}
        
        T operator*() const { return coro.promise().current_value_; }
        iterator& operator++() {
            if (coro) {
                coro.resume();
                if (coro.done()) done = true;
            }
            return *this;
        }
        bool operator!=(const iterator& other) const { return done != other.done; }
        bool operator==(const iterator& other) const { return done == other.done; }
    };
    
    Generator(std::coroutine_handle<promise_type> h) : handle_(h) {}
    Generator(Generator&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    ~Generator() { if (handle_) handle_.destroy(); }
    
    iterator begin() {
        if (handle_) {
            handle_.resume();
            if (handle_.done()) return end();
        }
        return iterator(handle_, true);
    }
    
    iterator end() { return iterator(nullptr, true); }

private:
    std::coroutine_handle<promise_type> handle_;
};

// ============================================================
// 生成器操作函数
// ============================================================

// Map: 转换每个元素
template<typename T, typename F>
Generator<std::invoke_result_t<F, T>> map(Generator<T> source, F func) {
    for (auto value : source) {
        co_yield func(value);
    }
}

// Filter: 过滤元素
template<typename T, typename F>
Generator<T> filter(Generator<T> source, F predicate) {
    for (auto value : source) {
        if (predicate(value)) {
            co_yield value;
        }
    }
}

// Take: 取前 n 个元素
template<typename T>
Generator<T> take(Generator<T> source, size_t n) {
    size_t count = 0;
    for (auto value : source) {
        if (count++ >= n) break;
        co_yield value;
    }
}

// Skip: 跳过前 n 个元素
template<typename T>
Generator<T> skip(Generator<T> source, size_t n) {
    size_t count = 0;
    for (auto value : source) {
        if (count++ >= n) {
            co_yield value;
        }
    }
}

// FlatMap: 展开嵌套生成器
template<typename T, typename F>
Generator<std::invoke_result_t<F, T>> flat_map(Generator<T> source, F func) {
    for (auto value : source) {
        for (auto sub_value : func(value)) {
            co_yield sub_value;
        }
    }
}

// TakeWhile: 取满足条件的元素直到不满足为止
template<typename T, typename F>
Generator<T> take_while(Generator<T> source, F predicate) {
    for (auto value : source) {
        if (!predicate(value)) break;
        co_yield value;
    }
}

// ============================================================
// 范围生成器
// ============================================================
Generator<int> range(int start, int end, int step = 1) {
    for (int i = start; i < end; i += step) {
        co_yield i;
    }
}

// 斐波那契
Generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield a;
        int next = a + b;
        a = b;
        b = next;
    }
}

// ============================================================
// 演示
// ============================================================
int main() {
    std::cout << "=== Generator Pipeline Demo ===\n\n";
    
    // 基本流水线: range -> filter -> map -> take
    std::cout << "--- 流水线: 1-20 中偶数 * 2 ---\n";
    auto result = map(
        filter(
            range(1, 21),
            [](int x) { return x % 2 == 0; }
        ),
        [](int x) { return x * 2; }
    );
    
    std::cout << "结果: ";
    for (int value : result) {
        std::cout << value << " ";
    }
    std::cout << "\n";
    
    // 斐波那契流水线
    std::cout << "\n--- 斐波那契前 10 项 ---\n";
    auto fibs = take(fibonacci(), 10);
    std::cout << "结果: ";
    for (int f : fibs) {
        std::cout << f << " ";
    }
    std::cout << "\n";
    
    // 复杂流水线
    std::cout << "\n--- 复杂流水线: 斐波那契中偶数项的平方 ---\n";
    auto complex = map(
        filter(fibonacci(), [](int x) { return x % 2 == 0; }),
        [](int x) { return x * x; }
    );
    
    std::cout << "前 5 个: ";
    auto count = 0;
    for (int value : complex) {
        std::cout << value << " ";
        if (++count >= 5) break;
    }
    std::cout << "\n";
    
    // FlatMap 示例
    std::cout << "\n--- FlatMap: 展开嵌套 ---\n";
    auto nested = flat_map(
        range(1, 4),
        [](int x) -> Generator<int> {
            for (int i = 1; i <= x; ++i) {
                co_yield x * 10 + i; // 11, 12, 21, 22, 23, 31, 32, 33
            }
        }
    );
    
    std::cout << "展开后: ";
    for (int value : nested) {
        std::cout << value << " ";
    }
    std::cout << "\n";
    
    std::cout << "\n=== 生成器流水线演示完成 ===\n";
    
    return 0;
}
