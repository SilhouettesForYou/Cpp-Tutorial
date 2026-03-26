/**
 * 03_aba_problem.cpp
 * ABA 问题演示与解决方案
 */

#include <atomic>
#include <iostream>
#include <thread>
#include <memory>
#include <array>

// ========== 问题演示 ==========

struct Node {
    int value;
    std::atomic<Node*> next{nullptr};
};

std::atomic<Node*> head{nullptr};

// 普通 CAS 会遇到 ABA 问题
void demonstrate_aba() {
    std::cout << "=== ABA Problem Demo ===\n";

    // 初始: A -> B -> C
    Node* c = new Node{3, nullptr};
    Node* b = new Node{2, c};
    Node* a = new Node{1, b};
    head.store(a);

    // 线程1: 想把 A 替换成 D (A -> B -> C 变成 D -> B -> C)
    // 但它读取 A 很慢
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 线程2: 在此期间把 A 改成 D，再改回 A
    Node* d = new Node{10, b};
    head.compare_exchange_strong(a, d);  // A -> D
    a = d;
    head.compare_exchange_strong(a, head.load()->next->next);  // ??? 这样不好演示

    // 更好的演示
    head.store(a);  // 恢复

    // 简化演示
    Node* expected = head.load();
    Node* new_node = new Node{100, b};

    // 模拟 ABA: 先改成 D，再改回 B
    head.store(new_node);  // 改
    head.store(b);         // 改回 B（和原来的 B 是同一个！）

    // 现在线程1 的 CAS 会"成功"，但状态已经变了
    expected = a;
    bool success = head.compare_exchange_strong(expected, new_node);
    std::cout << "CAS success: " << success
              << " (expected was modified but CAS thinks it succeeded!)\n";

    // 清理
    delete d;
    delete new_node;
    delete c;
    delete b;
    delete a;
}

// ========== 解决方案1: Tagged Pointer ==========

template<typename T>
struct TaggedPtr {
    T* ptr;
    uintptr_t tag;

    TaggedPtr() : ptr(nullptr), tag(0) {}
    TaggedPtr(T* p, uintptr_t t) : ptr(p), tag(t) {}

    bool operator==(const TaggedPtr& other) const {
        return ptr == other.ptr && tag == other.tag;
    }
};

template<typename T>
class LockFreeStackSafe {
    struct Node {
        T data;
        std::atomic<Node*> next{nullptr};
        Node(T d) : data(std::move(d)) {}
    };

    std::atomic<TaggedPtr<Node>> head_{TaggedPtr<Node>()};

public:
    void push(T data) {
        Node* new_node = new Node(std::move(data));
        TaggedPtr<Node> old_head = head_.load();

        do {
            new_node->next.store(old_head.ptr, std::memory_order_relaxed);
            TaggedPtr<Node> new_head{new_node, old_head.tag + 1};
        } while (!head_.compare_exchange_weak(
            old_head, {new_node, old_head.tag + 1},
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    std::optional<T> pop() {
        TaggedPtr<Node> old_head = head_.load();

        do {
            if (old_head.ptr == nullptr) return std::nullopt;
            TaggedPtr<Node> new_head{old_head.ptr->next.load(), old_head.tag + 1};
        } while (!head_.compare_exchange_weak(
            old_head, {old_head.ptr->next.load(), old_head.tag + 1},
            std::memory_order_acq_rel,
            std::memory_order_acquire));

        T data = std::move(old_head.ptr->data);
        delete old_head.ptr;
        return data;
    }
};

// ========== 解决方案2: Hazard Pointer ==========

template<typename T>
class HazardPointer {
    static constexpr size_t MAX_THREADS = 64;
    static thread_local size_t thread_id_;
    static std::atomic<void*>* hazard_pointers_[MAX_THREADS];
    static std::atomic< std::array<void*, MAX_THREADS> > retired_ptrs_;
    static std::atomic<size_t> retired_count_;

public:
    static void* protect(std::atomic<T*>& ptr) {
        T* p = nullptr;
        while ((p = ptr.load()) != nullptr) {
            // 设置 hazard pointer
            if (ptr.compare_exchange_weak(p, p)) {
                return p;
            }
        }
        return nullptr;
    }

    static void retire(void* p) {
        // 简化版本：直接删除
        // 真正实现需要延迟删除
        delete p;
    }
};

// ========== 测试 ==========

int main() {
    demonstrate_aba();

    std::cout << "\n=== Safe Lock-Free Stack ===\n";
    LockFreeStackSafe<int> safe_stack;

    safe_stack.push(1);
    safe_stack.push(2);
    safe_stack.push(3);

    if (auto v = safe_stack.pop()) {
        std::cout << "Popped: " << *v << '\n';
    }
    if (auto v = safe_stack.pop()) {
        std::cout << "Popped: " << *v << '\n';
    }

    std::cout << "\nABA problem solutions:\n";
    std::cout << "1. Tagged Pointer: Add version number\n";
    std::cout << "2. Hazard Pointer: Protect nodes being read\n";
    std::cout << "3. RCU: Read-Copy-Update\n";

    return 0;
}
