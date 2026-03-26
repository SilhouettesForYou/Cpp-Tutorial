/**
 * 02_atomic_flag.cpp
 * atomic_flag 门锁演示
 */

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

// 使用 atomic_flag 实现自旋锁
class SpinLock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

public:
    void lock() {
        // test_and_set 返回旧值，true 表示已被占用（自旋）
        while (flag.test_and_set(std::memory_order_acquire)) {
            // 空循环，自旋等待
        }
    }

    void unlock() {
        flag.clear(std::memory_order_release);
    }
};

SpinLock spin;

void critical_section(int id) {
    spin.lock();
    std::cout << "Thread " << id << " entered critical section\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::cout << "Thread " << id << " leaving critical section\n";
    spin.unlock();
}

// atomic_flag 作为一次性事件
void test_once_event() {
    std::atomic_flag ready = ATOMIC_FLAG_INIT;

    std::thread t([&ready]() {
        std::cout << "Worker: waiting...\n";
        while (ready.test_and_set() == false) {
            // 自旋等待
        }
        std::cout << "Worker: ready!\n";
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ready.test_and_set();  // 设置为 true，唤醒 worker
    t.join();
}

int main() {
    std::cout << "=== SpinLock ===\n";
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(critical_section, i);
    }
    for (auto& t : threads) {
        t.join();
    }

    std::cout << "\n=== Once Event ===\n";
    test_once_event();

    return 0;
}
