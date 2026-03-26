/**
 * 01_atomic_basic.cpp
 * atomic 基本操作演示
 */

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

void test_basic_atomic() {
    std::atomic<int> counter{0};

    // store: 原子写入
    counter.store(10);
    std::cout << "After store(10): " << counter.load() << '\n';

    // fetch_add: 原子加法，返回旧值
    int old = counter.fetch_add(5);
    std::cout << "fetch_add(5) returned: " << old
              << ", counter = " << counter.load() << '\n';

    // fetch_sub: 原子减法，返回旧值
    old = counter.fetch_sub(3);
    std::cout << "fetch_sub(3) returned: " << old
              << ", counter = " << counter.load() << '\n';

    // exchange: 原子交换，总是返回旧值
    old = counter.exchange(100);
    std::cout << "exchange(100) returned: " << old
              << ", counter = " << counter.load() << '\n';
}

void test_bool_atomic() {
    std::atomic<bool> flag{false};

    // store
    flag.store(true);
    std::cout << "Flag is: " << std::boolalpha << flag.load() << '\n';

    // exchange
    bool old = flag.exchange(false);
    std::cout << "exchange returned: " << old << ", flag = " << flag.load() << '\n';
}

void test_pointer_atomic() {
    int arr[] = {1, 2, 3, 4, 5};
    std::atomic<int*> ptr{&arr[0]};

    // 指针算术运算
    int* old = ptr.fetch_add(2);  // 移动 2 个 int
    std::cout << "fetch_add(2) returned: " << *old
              << ", current: " << *ptr.load() << '\n';

    // 也可以用 ++
    ++ptr;
    std::cout << "After ++ptr: " << *ptr.load() << '\n';
}

void test_multithread_counter() {
    std::atomic<int> counter{0};
    const int NUM_THREADS = 4;
    const int INCREMENTS_PER_THREAD = 100000;

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&counter, INCREMENTS_PER_THREAD]() {
            for (int j = 0; j < INCREMENTS_PER_THREAD; ++j) {
                counter.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Final counter: " << counter.load()
              << " (expected: " << NUM_THREADS * INCREMENTS_PER_THREAD << ")\n";
}

int main() {
    std::cout << "=== Basic atomic ===\n";
    test_basic_atomic();

    std::cout << "\n=== Bool atomic ===\n";
    test_bool_atomic();

    std::cout << "\n=== Pointer atomic ===\n";
    test_pointer_atomic();

    std::cout << "\n=== Multi-thread counter ===\n";
    test_multithread_counter();

    return 0;
}
