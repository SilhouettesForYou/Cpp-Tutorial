// ============================================================
// 示例 8: parallel_await - 并行等待多个协程
// 使用 when_all 并行执行多个协程
// 适用于并发请求、数据并行处理
// ============================================================

#include <coroutine>
#include <iostream>
#include <vector>
#include <optional>
#include <future>

template <typename T>
struct parallel_task {
    struct promise_type {
        std::optional<T> value;
        std::exception_ptr exception;
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        parallel_task get_return_object() { return parallel_task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        void unhandled_exception() { exception = std::current_exception(); }
        template <typename V> void return_value(V&& v) { value = std::forward<V>(v); }
    };

    std::coroutine_handle<promise_type> h;
    parallel_task(std::coroutine_handle<promise_type> h) : h(h) {}
    ~parallel_task() { if (h) h.destroy(); }
    T get() { return std::move(h.promise().value.value()); }
};

// when_all: 并行执行多个任务
template <typename... Ts>
struct when_all_result {
    std::tuple<Ts...> values;
};

template <typename... Ts>
when_all_result<Ts...> when_all(parallel_task<Ts>&... tasks) {
    (tasks.h.resume(), ...);  // C++17 fold expression - 启动所有协程
    // 等待所有完成（简化版本）
    return {{std::move(tasks.h.promise().value.value())...}};
}

// 并行任务示例
parallel_task<int> fetch_data(int id) {
    std::cout << "[Fetch " << id << "] 开始获取数据\n";
    co_await std::suspend_always{};
    std::cout << "[Fetch " << id << "] 数据获取完成\n";
    co_return id * 100;
}

parallel_task<std::string> fetch_string(int id) {
    std::cout << "[FetchStr " << id << "] 开始获取字符串\n";
    co_await std::suspend_always{};
    std::cout << "[FetchStr " << id << "] 获取完成\n";
    co_return "String_" + std::to_string(id);
}

int main() {
    std::cout << "[Main] 创建并行任务\n";
    parallel_task<int> t1 = fetch_data(1);
    parallel_task<int> t2 = fetch_data(2);
    parallel_task<int> t3 = fetch_data(3);

    std::cout << "[Main] 等待所有任务完成...\n";
    // 模拟并行执行
    t1.h.resume();
    t2.h.resume();
    t3.h.resume();

    std::cout << "[Main] t1 = " << t1.get() << "\n";
    std::cout << "[Main] t2 = " << t2.get() << "\n";
    std::cout << "[Main] t3 = " << t3.get() << "\n";

    // 混合类型示例
    auto [s1, s2] = when_all(fetch_string(10), fetch_string(20));
    std::cout << "[Main] s1 = " << s1 << ", s2 = " << s2 << "\n";

    return 0;
}
