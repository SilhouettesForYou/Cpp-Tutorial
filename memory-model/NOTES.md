# C++ 内存模型学习笔记

> 涵盖：C++11/14/17/20 内存模型、原子操作、内存顺序、内存屏障、无锁编程

---

## 目录

1. [内存模型基础](#1-内存模型基础)
2. [原子类型与 atomic](#2-原子类型与-atomic)
3. [内存顺序 (Memory Order)](#3-内存顺序-memory-order)
4. [内存屏障 (Fences)](#4-内存屏障-fences)
5. [无锁编程 (Lock-Free)](#5-无锁编程-lock-free)
6. [常见陷阱](#6-常见陷阱)
7. [速查表](#7-速查表)

---

## 1. 内存模型基础

### 1.1 什么是内存模型？

内存模型定义了：
- **可见性**：一个线程的写入，何时对另一个线程可见
- **顺序性**：操作在多线程下是否按代码顺序执行
- **原子性**：某些操作是否不可分割

### 1.2 为什么需要内存模型？

```
// 编译器优化前
x = 10;       // 可能被重排
flag = true;  // 可能被重排

// 编译器优化后（违反预期）
flag = true;  // 先执行
x = 10;       // 后执行
```

没有内存模型，编译器和 CPU 会自由重排指令，导致并发程序出错。

### 1.3 happens-before 关系

| 关系 | 含义 |
|------|------|
| `hb(a, b)` | 操作 a 在 b 之前完成，b 能看到 a 的结果 |
| `dr(a, b)` | 数据竞争：同一内存位置至少一个写 + 非原子访问 |

**数据竞争消除法则**：程序无数据竞争 = 所有对共享变量的访问都是原子的 + 有正确的 happens-before 关系。

---

## 2. 原子类型与 atomic

### 2.1 基本用法

```cpp
#include <atomic>

std::atomic<int> counter{0};
std::atomic<bool> ready{false};

counter.fetch_add(1);      // 原子 +1
ready.store(true);         // 原子写入
int x = counter.load();    // 原子读取
```

### 2.2 atomic 支持的类型

| 类型 | 说明 |
|------|------|
| `atomic<bool>` | 布尔原子 |
| `atomic<int>` | 整数原子 |
| `atomic<T*>` | 指针原子 |
| `atomic<float>` | C++20 |
| `atomic<shared_ptr<T>>` | 智能指针原子 |
| `atomic_ref<T>` | 引用包装（C++20）|

### 2.3 非原子类型的 atomic 特化

```cpp
std::atomic<int*> ptr;      // 特化版本
std::atomic<int> flags;     // 特化版本（有 fetch_add 等）
```

### 2.4 atomic 常用操作

| 操作 | 说明 |
|------|------|
| `load()` | 原子读取 |
| `store(val)` | 原子写入 |
| `exchange(val)` | 原子交换，返回旧值 |
| `fetch_add(val)` | 原子加法 |
| `fetch_sub(val)` | 原子减法 |
| `fetch_and(val)` | 原子 AND |
| `fetch_or(val)` | 原子 OR |
| `compare_exchange_weak()` | CAS（可能伪失败）|
| `compare_exchange_strong()` | CAS（保证不伪失败）|

---

## 3. 内存顺序 (Memory Order)

### 3.1 六种内存顺序

```cpp
namespace std::memory_order {
    seq_cst,    // 顺序一致性（默认）
    acquire,    // 获取
    release,    // 释放
    acq_rel,    // 获取-释放
    relaxed,    // 松弛
    consume,    // 依赖排序（C++20 废弃）
}
```

### 3.2 顺序详解

#### `seq_cst` - 顺序一致性

最严格，保证：
- 所有线程看到相同的操作顺序
- 所有原子操作全局有序
- **默认选择**，性能开销最大

```cpp
std::atomic<int> x{0}, y{0};

// Thread 1
x.store(1, std::memory_order_seq_cst);
y.store(1, std::memory_order_seq_cst);

// Thread 2
while (y.load(std::memory_order_seq_cst) == 0);
assert(x.load(std::memory_order_seq_cst) == 1);  // 永远成立
```

#### `acquire` - 获取

- 用于读取（load）
- 保证之前的所有操作**都**在当前线程可见
- 常用于锁释放后的同步

```cpp
std::atomic<bool> ready{false};
int data = 0;

// Thread 1
data = 42;
ready.store(true, std::memory_order_release);

// Thread 2
while (!ready.load(std::memory_order_acquire));
assert(data == 42);  // 一定成立
```

#### `release` - 释放

- 用于写入（store）
- 保证当前操作**之后**的所有操作都对其他线程可见（如果它们用 acquire 同步）
- 常用于锁获取前的写入

#### `acq_rel` - 获取-释放

- 用于 read-modify-write 操作（如 `exchange`）
- 同时具有 acquire 和 release 的效果

```cpp
std::atomic<int> queue{0};

// Thread 1: 生产
queue.store(1, std::memory_order_release);

// Thread 2: 消费
int old = queue.exchange(2, std::memory_order_acq_rel);
```

#### `relaxed` - 松弛

- 只保证**原子性**，不保证顺序
- 允许编译器/CPU 重排
- 适合计数器等不需要同步的场景

```cpp
std::atomic<int> counter{0};

// 多线程执行，结果正确（++ 本身是原子的）
counter.fetch_add(1, std::memory_order_relaxed);
```

### 3.3 顺序对比

| 顺序 | 可见性 | 顺序保证 | 性能 |
|------|--------|----------|------|
| `seq_cst` | 全局 | 强 | 最慢 |
| `acq_rel` | 配对 | 中 | 中 |
| `acquire` / `release` | 配对 | 中 | 中 |
| `relaxed` | 无 | 无 | 最快 |

---

## 4. 内存屏障 (Fences)

### 4.1 三种 fence

```cpp
std::atomic_thread_fence(std::memory_order_seq_cst);      // 顺序一致性 fence
std::atomic_thread_fence(std::memory_order_acquire);        // acquire fence
std::atomic_thread_fence(std::memory_order_release);        // release fence
```

### 4.2 fence vs atomic 操作

```cpp
// 方式1: fence
std::atomic<bool> ready{false};
int data = 0;

void producer() {
    data = 42;
    std::atomic_thread_fence(std::memory_order_release);
    ready.store(true, std::memory_order_relaxed);
}

void consumer() {
    while (!ready.load(std::memory_order_relaxed));
    std::atomic_thread_fence(std::memory_order_acquire);
    assert(data == 42);
}

// 方式2: 直接用 acquire/release（更简洁）
void producer2() {
    data = 42;
    ready.store(true, std::memory_order_release);
}

void consumer2() {
    while (!ready.load(std::memory_order_acquire));
    assert(data == 42);
}
```

### 4.3 fence 应用场景

- **分离读写位置**：不能用单个 atomic 配对时
- **性能调优**：在特定点插入 fence 而非全局使用 seq_cst
- **混合同步**：同时同步多变量

---

## 5. 无锁编程 (Lock-Free)

### 5.1 compare_exchange_weak vs strong

```cpp
// weak: 可能在无事发生时返回 false（伪失败），但更快
// strong: 保证只在真正失败时返回 false

std::atomic<int> count{0};

bool CAS_weak(int expected) {
    int current = expected;
    return count.compare_exchange_weak(current, expected + 1);
}

bool CAS_strong(int expected) {
    int current = expected;
    return count.compare_exchange_strong(current, expected + 1);
}
```

**选择建议**：
- 循环中使用 → `weak`（性能更好）
- 单次尝试中使用 → `strong`

### 5.2 无锁栈 (Lock-Free Stack)

```cpp
template<typename T>
class LockFreeStack {
    struct Node {
        T data;
        Node* next;
        Node(T d) : data(std::move(d)), next(nullptr) {}
    };

    std::atomic<Node*> head_;

public:
    void push(T data) {
        Node* new_node = new Node(std::move(data));
        new_node->next = head_.load(std::memory_order_relaxed);
        while (!head_.compare_exchange_weak(
            new_node->next, new_node,
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    std::optional<T> pop() {
        Node* head = head_.load(std::memory_order_acquire);
        while (head && !head_.compare_exchange_weak(
            head, head->next,
            std::memory_order_relaxed,
            std::memory_order_acquire));
        if (!head) return std::nullopt;
        T data = std::move(head->data);
        delete head;
        return data;
    }
};
```

### 5.3 ABA 问题

**问题**：线程 A 读取 X 为 A，线程 B 改为 B 再改回 A，线程 A 的 CAS 成功但状态已变化。

**解决方案**：
1. **Tagged Pointer**：附加版本号
2. **Delayed Delete**：不立即删除节点
3. ** Hazard Pointer**：保护正在访问的节点

```cpp
//Tagged Pointer 解决 ABA
struct TaggedPtr {
    Node* ptr;
    uintptr_t tag;
};

std::atomic<TaggedPtr> head;

bool CAS(Node* expected, Node* new_node) {
    TaggedPtr current = head.load();
    if (current.ptr == expected) {
        return head.compare_exchange_strong(current,
            {new_node, current.tag + 1});
    }
    return false;
}
```

---

## 6. 常见陷阱

### 6.1 误用 relaxed 导致数据竞争

```cpp
// ❌ 错误：没有同步
std::atomic<bool> ready{false};
int data = 0;

void writer() {
    data = 42;
    ready.store(true, std::memory_order_relaxed);  // 没有同步！
}

void reader() {
    while (!ready.load(std::memory_order_relaxed));  // 可能永远看不到 42
    assert(data == 42);
}

// ✅ 正确：使用 release-acquire
void writer2() {
    data = 42;
    ready.store(true, std::memory_order_release);
}

void reader2() {
    while (!ready.load(std::memory_order_acquire));
    assert(data == 42);
}
```

### 6.2 过度使用 seq_cst

```cpp
// ❌ 低效：为每个操作使用 seq_cst
counter1.store(1, std::memory_order_seq_cst);
counter2.store(2, std::memory_order_seq_cst);

// ✅ 效率：用 relaxed 替代（如果只关心计数）
counter1.store(1, std::memory_order_relaxed);
counter2.store(2, std::memory_order_relaxed);
```

### 6.3 死锁

```cpp
// ❌ 错误：两把锁顺序不一致
// Thread 1
m1.lock();
m2.lock();

// Thread 2
m2.lock();
m1.lock();

// ✅ 正确：始终按相同顺序加锁
// Thread 1 & 2
m1.lock();
m2.lock();
```

### 6.4 伪失败导致死循环

```cpp
// ❌ 错误：用 strong 可能在某些架构上性能差
while (!ptr.compare_exchange_strong(expected, new_val));

// ✅ 正确：用 weak + 循环
do {
    expected = ptr.load();
    // ... 检查逻辑
} while (!ptr.compare_exchange_weak(expected, new_val,
    std::memory_order_release, std::memory_order_relaxed));
```

---

## 7. 速查表

### 7.1 atomic 操作选择

| 场景 | 推荐顺序 |
|------|----------|
| 计数器（只递增）| `relaxed` |
| 标志位（单生产者-单消费者）| `relaxed` |
| 标志位（多生产者-多消费者）| `release`/`acquire` |
| 复杂数据结构同步 | `seq_cst` |
| 单次交换（锁实现）| `acq_rel` |

### 7.2 同步模式

```cpp
// 单向同步 (writer → reader)
writer: store(x, release)
reader: while (!ready.load(acquire)) {}

// 双向同步 (互斥)
lock:  exchange(_, acq_rel)
unlock: store(_, release)

// 顺序一致性
x.store(1, seq_cst)
y.store(1, seq_cst)
```

### 7.3 fence 等价

```cpp
// fence(acquire) ≈ load(acquire)
// fence(release) ≈ store(release)
// fence(seq_cst) ≈ store(seq_cst) + load(seq_cst) + 全局序
```

---

## 示例文件

| 文件 | 内容 |
|------|------|
| `01_basics/01_atomic_basic.cpp` | atomic 基本操作 |
| `02_atomic/01_atomic_ops.cpp` | 所有原子操作示例 |
| `02_atomic/02_atomic_flag.cpp` | atomic_flag 门锁 |
| `03_mo/01_memory_order.cpp` | 各种内存顺序对比 |
| `03_mo/02_producer_consumer.cpp` | 生产者-消费者模型 |
| `04_fences/01_fence_basics.cpp` | fence 基础用法 |
| `05_lockfree/01_cas.cpp` | CAS 操作 |
| `05_lockfree/02_lockfree_stack.cpp` | 无锁栈 |
| `05_lockfree/03_aba_problem.cpp` | ABA 问题演示 |
