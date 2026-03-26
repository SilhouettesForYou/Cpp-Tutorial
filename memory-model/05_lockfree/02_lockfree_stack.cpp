/**
 * 02_lockfree_stack.cpp
 * 无锁栈实现
 */

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include <memory>
#include <optional>

template<typename T>
class LockFreeStack {
private:
    struct Node {
        T data;
        std::atomic<Node*> next{nullptr};
        Node(T d) : data(std::move(d)) {}
    };

    std::atomic<Node*> head_{nullptr};

public:
    ~LockFreeStack() {
        while (auto node = pop()) {
            delete node.value().data;  // 假设 T 是指针类型
        }
    }

    // 入栈：头插法
    void push(T data) {
        Node* new_node = new Node(std::move(data));
        new_node->next.store(head_.load(std::memory_order_relaxed),
                            std::memory_order_relaxed);

        // 使用 CAS 确保原子性
        while (!head_.compare_exchange_weak(
            new_node->next, new_node,
            std::memory_order_release,
            std::memory_order_relaxed)) {
            // head 被其他线程修改，new_node->next 已更新为最新 head
            // 重试
        }
    }

    // 出栈
    std::optional<T> pop() {
        Node* head = head_.load(std::memory_order_acquire);

        // 循环直到成功
        while (head != nullptr &&
               !head_.compare_exchange_weak(
                   head, head->next.load(std::memory_order_relaxed),
                   std::memory_order_acq_rel,
                   std::memory_order_acquire)) {
            // head 已被其他线程修改，更新为最新值后重试
        }

        if (head == nullptr) {
            return std::nullopt;
        }

        T data = std::move(head->data);
        delete head;
        return data;
    }

    bool is_empty() const {
        return head_.load(std::memory_order_acquire) == nullptr;
    }
};

// 测试
int main() {
    std::cout << "=== Lock-Free Stack ===\n";

    LockFreeStack<int*> stack;

    // 多线程入栈
    std::vector<std::thread> push_threads;
    for (int t = 0; t < 4; ++t) {
        push_threads.emplace_back([&stack, t]() {
            for (int i = 0; i < 1000; ++i) {
                stack.push(new int(t * 1000 + i));
            }
        });
    }

    for (auto& t : push_threads) {
        t.join();
    }

    std::cout << "Pushed 4000 elements\n";

    // 多线程出栈
    std::atomic<int> pop_count{0};
    std::vector<std::thread> pop_threads;
    for (int t = 0; t < 4; ++t) {
        pop_threads.emplace_back([&stack, &pop_count]() {
            while (auto node = stack.pop()) {
                delete node.value();
                pop_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : pop_threads) {
        t.join();
    }

    std::cout << "Popped " << pop_count.load() << " elements\n";
    std::cout << "Stack is empty: " << std::boolalpha << stack.is_empty() << '\n';

    return 0;
}
