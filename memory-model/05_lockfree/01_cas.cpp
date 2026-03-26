/**
 * 01_cas.cpp
 * Compare-And-Swap 操作演示
 */

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include <random>

// 简单的 CAS 循环
void test_cas_loop() {
    std::cout << "=== CAS Loop ===\n";

    std::atomic<int> counter{0};

    auto increment = [&]() {
        for (int i = 0; i < 10000; ++i) {
            int expected = counter.load(std::memory_order_relaxed);
            while (!counter.compare_exchange_weak(
                expected, expected + 1,
                std::memory_order_relaxed,
                std::memory_order_relaxed)) {
                // CAS 失败，expected 已被更新为当前值，重试
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(increment);
    }
    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Final counter: " << counter.load()
              << " (expected: 40000)\n";
}

// CAS 用于实现其他原子操作
void test_cas_custom_ops() {
    std::cout << "\n=== CAS Custom Operations ===\n";

    std::atomic<int> value{100};

    // 实现 max 操作
    auto fetch_max = [&](int new_val) {
        int current = value.load();
        while (current < new_val &&
               !value.compare_exchange_weak(
                   current, new_val,
                   std::memory_order_seq_cst,
                   std::memory_order_relaxed)) {
            // current 已被更新为最新值，继续尝试
        }
        return current;
    };

    std::cout << "Initial: " << value.load() << '\n';
    int old = fetch_max(150);
    std::cout << "fetch_max(150) returned: " << old
              << ", current: " << value.load() << '\n';

    old = fetch_max(120);  // 不会更新，因为 120 < 150
    std::cout << "fetch_max(120) returned: " << old
              << ", current: " << value.load() << '\n';

    old = fetch_max(200);
    std::cout << "fetch_max(200) returned: " << old
              << ", current: " << value.load() << '\n';
}

// 伪失败演示
void test_weak_vs_strong() {
    std::cout << "\n=== Weak vs Strong CAS ===\n";

    std::atomic<int> val{0};

    // weak 可能在无事发生时返回 false
    int fail_count_weak = 0;
    for (int i = 0; i < 1000; ++i) {
        int expected = 0;
        if (!val.compare_exchange_weak(expected, 1,
                                      std::memory_order_relaxed,
                                      std::memory_order_relaxed)) {
            // expected 被更新为当前值
            fail_count_weak++;
            expected = 0;  // 重置
        }
    }
    std::cout << "Weak CAS failures: " << fail_count_weak << '\n';

    // strong 只在真正失败时返回 false（某些架构可能内部循环）
    val = 0;
    int fail_count_strong = 0;
    for (int i = 0; i < 1000; ++i) {
        int expected = 0;
        if (!val.compare_exchange_strong(expected, 1,
                                        std::memory_order_relaxed,
                                        std::memory_order_relaxed)) {
            fail_count_strong++;
            expected = 0;
        }
    }
    std::cout << "Strong CAS failures: " << fail_count_strong << '\n';
}

int main() {
    test_cas_loop();
    test_cas_custom_ops();
    test_weak_vs_strong();

    return 0;
}
