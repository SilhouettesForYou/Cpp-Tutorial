/**
 * 01_fence_basics.cpp
 * 内存屏障基础演示
 */

#include <atomic>
#include <iostream>
#include <thread>
#include <cassert>

// 全局变量
std::atomic<int> x{0}, y{0};
std::atomic<int> r1{0}, r2{0};

// 没有 fence 的情况（可能被重排）
void without_fence() {
    std::cout << "=== Without Fence ===\n";
    x.store(1, std::memory_order_relaxed);
    y.store(1, std::memory_order_relaxed);

    // 两个 load 可能都看到旧值
}

// 使用 fence 强制顺序
void with_fence() {
    std::cout << "\n=== With Fence ===\n";
    x.store(1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    y.store(1, std::memory_order_relaxed);
}

// fence 实现 release-acquire
std::atomic<bool> ready{false};
int data = 0;

void fence_producer() {
    data = 42;
    // fence(release) 效果等同于 store(release)
    std::atomic_thread_fence(std::memory_order_release);
    ready.store(true, std::memory_order_relaxed);
}

void fence_consumer() {
    while (!ready.load(std::memory_order_relaxed)) {
        std::this_thread::yield();
    }
    std::atomic_thread_fence(std::memory_order_acquire);
    // 此时一定能看到 data = 42
    assert(data == 42);
    std::cout << "Consumer saw data=" << data << '\n';
}

// fence 实现互斥
std::atomic<int> turn{0};

void process_a() {
    // 获取 turn
    while (turn.load(std::memory_order_relaxed) != 0) {
        std::this_thread::yield();
    }
    std::atomic_thread_fence(std::memory_order_acquire);

    // 临界区
    std::cout << "A doing work\n";

    // 释放 turn
    std::atomic_thread_fence(std::memory_order_release);
    turn.store(1, std::memory_order_relaxed);
}

void process_b() {
    while (turn.load(std::memory_order_relaxed) != 1) {
        std::this_thread::yield();
    }
    std::atomic_thread_fence(std::memory_order_acquire);

    std::cout << "B doing work\n";

    std::atomic_thread_fence(std::memory_order_release);
    turn.store(0, std::memory_order_relaxed);
}

int main() {
    std::cout << "=== Fence Producer-Consumer ===\n";
    ready = false;
    data = 0;

    std::thread p(fence_producer);
    std::thread c(fence_consumer);
    p.join();
    c.join();

    std::cout << "\n=== Turn-based Processing ===\n";
    turn = 0;

    std::thread t1(process_a);
    std::thread t2(process_b);

    t1.join();
    t2.join();

    return 0;
}
