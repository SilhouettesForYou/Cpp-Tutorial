/**
 * 02_producer_consumer.cpp
 * 生产者-消费者模型
 */

#include <atomic>
#include <iostream>
#include <thread>
#include <queue>
#include <chrono>
#include <cassert>

template<typename T>
class RingBuffer {
    std::vector<T> buffer_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
    std::atomic<size_t> count_{0};
    size_t capacity_;

public:
    explicit RingBuffer(size_t capacity) : buffer_(capacity), capacity_(capacity) {}

    bool push(T value) {
        size_t c = count_.load(std::memory_order_relaxed);
        if (c >= capacity_) return false;

        // release: 保证数据写入在索引更新之前
        size_t head = head_.load(std::memory_order_relaxed);
        buffer_[head] = std::move(value);
        head_.store((head + 1) % capacity_, std::memory_order_relaxed);
        count_.fetch_add(1, std::memory_order_release);

        return true;
    }

    bool pop(T& value) {
        size_t c = count_.load(std::memory_order_acquire);
        if (c == 0) return false;

        size_t tail = tail_.load(std::memory_order_relaxed);
        value = std::move(buffer_[tail]);
        tail_.store((tail + 1) % capacity_, std::memory_order_relaxed);
        count_.fetch_sub(1, std::memory_order_release);

        return true;
    }
};

// 简化的生产者-消费者
std::atomic<bool> data_ready{false};
std::atomic<int> shared_data{0};

void producer(int id) {
    for (int i = 0; i < 5; ++i) {
        shared_data.store(i + id * 100, std::memory_order_relaxed);
        // 内存屏障：确保数据在 ready 之前写入
        std::atomic_thread_fence(std::memory_order_release);
        data_ready.store(true, std::memory_order_relaxed);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void consumer(int id) {
    int last_value = -1;
    while (true) {
        // 自旋等待数据就绪
        while (!data_ready.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        int value = shared_data.load(std::memory_order_relaxed);
        std::cout << "Consumer " << id << " got: " << value << '\n';

        if (value == 499) {  // producer 5 发出了 499
            std::cout << "Consumer " << id << " finishing\n";
            break;
        }

        data_ready.store(false, std::memory_order_relaxed);
    }
}

int main() {
    std::cout << "=== Producer-Consumer ===\n";

    // 生产者-消费者示例
    std::thread p(producer, 1);
    std::thread c1(consumer, 1);
    std::thread c2(consumer, 2);

    p.join();
    c1.join();
    c2.join();

    std::cout << "Done!\n";

    return 0;
}
