/**
 * 01_atomic_ops.cpp
 * atomic 所有操作演示
 */

#include <atomic>
#include <iostream>

int main() {
    std::atomic<int> ai{0};
    std::atomic<unsigned long long> aull{0};

    // ===== 整型原子操作 =====
    std::cout << "=== Integer atomic operations ===\n";

    // fetch_add: 返回旧值
    ai.store(10);
    int old = ai.fetch_add(5);
    std::cout << "fetch_add(5): old=" << old << ", new=" << ai.load() << '\n';

    // fetch_sub: 返回旧值
    ai.store(10);
    old = ai.fetch_sub(3);
    std::cout << "fetch_sub(3): old=" << old << ", new=" << ai.load() << '\n';

    // fetch_and: 返回旧值
    ai.store(0b1100);
    old = ai.fetch_and(0b1010);
    std::cout << "fetch_and(1010): old=" << old << ", new=" << ai.load() << '\n';

    // fetch_or: 返回旧值
    ai.store(0b1100);
    old = ai.fetch_or(0b0011);
    std::cout << "fetch_or(0011): old=" << old << ", new=" << ai.load() << '\n';

    // fetch_xor: 返回旧值
    ai.store(0b1100);
    old = ai.fetch_xor(0b1010);
    std::cout << "fetch_xor(1010): old=" << old << ", new=" << ai.load() << '\n';

    // +=, -=, &=, |=, ^=, ++, --
    ai.store(10);
    ai += 5;
    std::cout << "After +=5: " << ai.load() << '\n';

    ai++;
    std::cout << "After ++: " << ai.load() << '\n';

    // ===== CAS (Compare-And-Swap) =====
    std::cout << "\n=== CAS operations ===\n";

    ai.store(10);

    // compare_exchange_strong: 成功返回 true
    int expected = 10;
    bool success = ai.compare_exchange_strong(expected, 20);
    std::cout << "CAS(10->20): success=" << std::boolalpha << success
              << ", expected was=" << expected << ", value=" << ai.load() << '\n';

    // compare_exchange_strong: 失败返回 false
    expected = 10;  // 注意：即使失败，expected 也会被更新为当前值
    success = ai.compare_exchange_strong(expected, 30);
    std::cout << "CAS(10->30): success=" << success
              << ", expected was=" << expected << ", value=" << ai.load() << '\n';

    // compare_exchange_weak: 可能伪失败
    ai.store(10);
    do {
        success = ai.compare_exchange_weak(expected, 40);
    } while (!success);  // 循环处理伪失败
    std::cout << "Weak CAS eventually: value=" << ai.load() << '\n';

    // ===== is_lock_free =====
    std::cout << "\n=== Lock-free check ===\n";
    std::cout << "atomic<int> is lock-free: "
              << std::atomic_is_lock_free(&ai) << '\n';

    // ===== 类型转换 =====
    std::cout << "\n=== Type conversion ===\n";
    aull.store(1ULL << 40);
    std::cout << "64-bit atomic: " << aull.load() << '\n';

    return 0;
}
