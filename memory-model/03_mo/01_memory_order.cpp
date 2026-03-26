/**
 * 01_memory_order.cpp
 * 内存顺序对比演示
 */

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>

// 全局变量用于测试
std::atomic<int> x{0}, y{0};

// ========== relaxed: 只保证原子性，不保证顺序 ==========
void test_relaxed() {
    std::cout << "=== Relaxed (no ordering) ===\n";

    x.store(0, std::memory_order_relaxed);
    y.store(0, std::memory_order_relaxed);

    int r1 = -1, r2 = -1;

    std::thread t1([&]() {
        x.store(1, std::memory_order_relaxed);
        r1 = y.load(std::memory_order_relaxed);
    });

    std::thread t2([&]() {
        y.store(1, std::memory_order_relaxed);
        r2 = x.load(std::memory_order_relaxed);
    });

    t1.join();
    t2.join();

    std::cout << "r1=" << r1 << ", r2=" << r2 << '\n';
    std::cout << "Both zero is possible with relaxed!\n";
}

// ========== release-acquire: 同步点 ==========
void test_release_acquire() {
    std::cout << "\n=== Release-Acquire ===\n";

    x.store(0, std::memory_order_relaxed);
    y.store(0, std::memory_order_relaxed);

    int r1 = -1, r2 = -1;

    std::thread t1([&]() {
        x.store(1, std::memory_order_release);
        r1 = y.load(std::memory_order_acquire);
    });

    std::thread t2([&]() {
        y.store(1, std::memory_order_release);
        r2 = x.load(std::memory_order_acquire);
    });

    t1.join();
    t2.join();

    std::cout << "r1=" << r1 << ", r2=" << r2 << '\n';
    std::cout << "Both zero is NOT possible with release-acquire!\n";
}

// ========== seq_cst: 全局顺序 ==========
void test_seq_cst() {
    std::cout << "\n=== Sequential Consistent ===\n";

    x.store(0, std::memory_order_seq_cst);
    y.store(0, std::memory_order_seq_cst);

    int r1 = -1, r2 = -1;

    std::thread t1([&]() {
        x.store(1, std::memory_order_seq_cst);
        r1 = y.load(std::memory_order_seq_cst);
    });

    std::thread t2([&]() {
        y.store(1, std::memory_order_seq_cst);
        r2 = x.load(std::memory_order_seq_seq_cst);
    });

    t1.join();
    t2.join();

    std::cout << "r1=" << r1 << ", r2=" << r2 << '\n';
}

// ========== 演示 happens-before 关系 ==========
std::atomic<bool> ready{false};
int data = 0;

void producer() {
    data = 42;  // 可能在 store 之后（编译器重排）
    ready.store(true, std::memory_order_release);
}

void consumer() {
    while (!ready.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    // 一定能看见 data = 42
    assert(data == 42);
    std::cout << "Consumer saw data=" << data << '\n';
}

void test_happens_before() {
    std::cout << "\n=== Happens-Before Demo ===\n";

    ready = false;
    data = 0;

    std::thread p(producer);
    std::thread c(consumer);

    p.join();
    c.join();

    std::cout << "Happens-before relationship verified!\n";
}

int main() {
    test_relaxed();
    test_release_acquire();
    test_happens_before();

    return 0;
}
