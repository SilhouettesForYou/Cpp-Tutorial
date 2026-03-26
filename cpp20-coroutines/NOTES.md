# C++20 协程学习笔记

> 整理自实践示例，覆盖协程核心概念 → 同步原语 → 高级模式

---

## 目录

1. [协程是什么](#1-协程是什么)
2. [三大关键字](#2-三大关键字)
3. [核心三角：Promise / Handle / Awaitable](#3-核心三角)
4. [Promise Type 详解](#4-promise-type-详解)
5. [Awaitable 协议详解](#5-awaitable-协议详解)
6. [Generator 生成器](#6-generator-生成器)
7. [Task 异步任务](#7-task-异步任务)
8. [调度器与线程池](#8-调度器与线程池)
9. [同步原语](#9-同步原语)
10. [高级模式](#10-高级模式)
11. [常见陷阱](#11-常见陷阱)
12. [最佳实践](#12-最佳实践)
13. [速查表](#13-速查表)

---

## 1. 协程是什么

### 普通函数 vs 协程

```
普通函数：
  调用 → 执行 → 返回（一次性）

协程：
  调用 → 执行 → 挂起（保留状态）→ 恢复 → 再次挂起 → ... → 最终结束
```

**核心特征**：
- 可以在执行中途**挂起**，把控制权交还给调用者
- 挂起时**保留完整状态**（局部变量、执行位置）
- 之后可以从挂起点**恢复执行**

### 为什么需要协程？

| 问题 | 传统方案 | 协程方案 |
|------|---------|---------|
| 异步回调地狱 | callback hell | `co_await` 线性写法 |
| 惰性序列 | 手动状态机 | `co_yield` 生成器 |
| 并发资源消耗 | 每任务一个线程 | 轻量协程，共用线程 |
| 代码可读性 | 到处是回调 | 像同步代码一样清晰 |

---

## 2. 三大关键字

### `co_await`

等待一个异步操作完成，挂起当前协程：

```cpp
Task<int> fetch_data() {
    // 挂起协程，等待 HTTP 请求完成
    auto response = co_await http_get("https://example.com");
    co_return response.status_code;
}
```

### `co_yield`

生成一个值并挂起，下次恢复时继续执行：

```cpp
Generator<int> counter(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i;   // 产出值，暂停
    }
    // 循环结束后协程结束
}
```

### `co_return`

协程结束，可以返回一个值（也可以不返回）：

```cpp
Task<std::string> greet(std::string name) {
    co_return "Hello, " + name;   // 设置结果并结束
}

Task<void> do_work() {
    // ... 做一些事
    co_return;  // 显式结束（可省略）
}
```

### 三者关系

```
co_yield v   ≡   co_await promise.yield_value(v)
co_return v  ≡   promise.return_value(v); co_await final_suspend()
```

---

## 3. 核心三角

C++20 协程由三个核心组件构成：

```
┌──────────────────────────────────────────────────────────┐
│                    协程核心三角                            │
│                                                          │
│        coroutine_handle<P>                               │
│        ┌─────────────┐                                   │
│        │  协程句柄   │   操控协程生命周期                  │
│        │  Handle     │   handle.resume()                 │
│        │             │   handle.destroy()                │
│        └──────┬──────┘   handle.done()                  │
│               │ 引用                                     │
│        ┌──────▼──────┐         ┌─────────────┐          │
│        │  Promise    │◄────────│  Awaitable  │          │
│        │  Type       │         │  (co_await) │          │
│        │             │  yield_ │             │          │
│        │ 控制协程行为 │  value  │ 自定义挂起  │          │
│        └─────────────┘         └─────────────┘          │
└──────────────────────────────────────────────────────────┘
```

---

## 4. Promise Type 详解

每个协程都有一个 `promise_type`，它控制协程的全部行为。

### 必须实现的成员

```cpp
struct promise_type {
    // ① 协程刚启动时的行为
    //    suspend_always = 创建后立即挂起（懒执行）
    //    suspend_never  = 创建后立即执行
    auto initial_suspend() -> std::suspend_always;

    // ② 协程结束时的行为
    //    suspend_always = 结束后挂起，等待外部 destroy（安全）
    //    suspend_never  = 结束后自动销毁（危险，可能 use-after-free）
    auto final_suspend() noexcept -> std::suspend_always;

    // ③ 创建协程返回对象
    ReturnType get_return_object();

    // ④ 异常处理
    void unhandled_exception();

    // ⑤ 返回值（二选一）
    void return_void();             // 无返回值协程
    void return_value(T value);     // 有返回值协程
};
```

### 可选成员

```cpp
struct promise_type {
    // co_yield 时调用
    auto yield_value(T value) -> std::suspend_always;

    // 内存分配（高级用法）
    void* operator new(std::size_t size);
    void  operator delete(void* ptr);

    // await_transform：拦截所有 co_await（高级）
    template<typename U>
    auto await_transform(U&& awaitable) -> decltype(auto);
};
```

### 完整生命周期

```
协程被调用
    │
    ▼
分配协程帧（堆）
    │
    ▼
构造 promise_type
    │
    ▼
get_return_object()  ← 创建返回给调用者的对象
    │
    ▼
co_await initial_suspend()
    │                   │
    │ 不挂起             │ 挂起（suspend_always）
    │                   ▼
    │            控制权返回给调用者
    │
    ▼
协程函数体开始执行
    │
    ├──── co_yield v ────► yield_value(v) ──► 挂起
    │
    ├──── co_await x ────► (await 协议) ──► 可能挂起
    │
    ├──── co_return v ───► return_value(v)
    │
    ▼
co_await final_suspend()
    │                   │
    │ 不挂起             │ 挂起（suspend_always）
    ▼                   ▼
自动销毁协程帧      等待外部 handle.destroy()
```

---

## 5. Awaitable 协议详解

任何实现了三个方法的类型都是 Awaitable：

```cpp
struct MyAwaitable {
    // ① 能否立即完成（不需要挂起）？
    //    true  = 直接执行 await_resume，不挂起
    //    false = 需要挂起，执行 await_suspend
    bool await_ready();

    // ② 挂起时执行（传入当前协程句柄）
    //    void 返回值  = 挂起，控制权返回给调用者
    //    bool 返回值  = true 继续挂起，false 立即恢复
    //    coroutine_handle<> 返回值 = 对称转移到指定协程
    void await_suspend(std::coroutine_handle<> h);

    // ③ 恢复时返回的值（就是 co_await expr 的结果）
    T await_resume();
};
```

### 内置 Awaitable

```cpp
std::suspend_always   // await_ready() = false，总是挂起
std::suspend_never    // await_ready() = true，从不挂起
```

### 三种 await_suspend 返回类型

```cpp
// 1. void：挂起，控制权回到调用者/resume 处
void await_suspend(std::coroutine_handle<> h) {
    schedule(h);  // 加入队列，稍后由调度器恢复
}

// 2. bool：动态决定是否挂起
bool await_suspend(std::coroutine_handle<> h) {
    if (ready_) { return false; }  // false = 不挂起，继续执行
    pending_ = h;
    return true;  // true = 挂起
}

// 3. coroutine_handle<>：对称转移（零开销）
std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) {
    continuation_ = h;
    return next_coroutine_;  // 转移到另一个协程执行
}
```

### 对称转移的重要性

```cpp
// 没有对称转移：可能导致栈溢出
Task<int> a() { co_return co_await b(); }
Task<int> b() { co_return co_await c(); }
// a 恢复 b，b 恢复 c... 每次恢复都消耗栈空间

// 使用对称转移：O(1) 栈
coroutine_handle<> await_suspend(coroutine_handle<> h) {
    return next_;  // 直接转移，不增加调用栈
}
```

---

## 6. Generator 生成器

### 核心实现

```cpp
template<typename T>
class Generator {
public:
    struct promise_type {
        T current_value_;

        // 初始挂起（懒执行）
        std::suspend_always initial_suspend() { return {}; }
        // 最终挂起（安全销毁）
        std::suspend_always final_suspend() noexcept { return {}; }

        Generator get_return_object() {
            return Generator(
                std::coroutine_handle<promise_type>::from_promise(*this)
            );
        }

        // co_yield v 时调用
        std::suspend_always yield_value(T value) {
            current_value_ = std::move(value);
            return {};  // 挂起
        }

        void return_void() {}
        void unhandled_exception() { throw; }
    };

    // 迭代器支持（用于 range-for）
    struct iterator { /* ... */ };
    iterator begin() { handle_.resume(); return {handle_}; }
    iterator end()   { return {nullptr}; }

private:
    std::coroutine_handle<promise_type> handle_;
};
```

### 使用示例

```cpp
// 无限序列
Generator<int> naturals(int start = 0) {
    for (int n = start; ; ++n) {
        co_yield n;
    }
}

// 有限序列
Generator<std::string> lines(std::istream& stream) {
    std::string line;
    while (std::getline(stream, line)) {
        co_yield line;
    }
}

// 使用
for (int n : naturals()) {
    if (n > 10) break;
    std::cout << n << "\n";  // 0 1 2 3 ... 10
}
```

### 生成器流水线

```cpp
// 惰性 map
template<typename T, typename F>
Generator<auto> map(Generator<T> source, F fn) {
    for (auto&& val : source) co_yield fn(val);
}

// 惰性 filter
template<typename T, typename F>
Generator<T> filter(Generator<T> source, F pred) {
    for (auto&& val : source) {
        if (pred(val)) co_yield val;
    }
}

// 链式使用
auto result = map(
    filter(range(1, 100), [](int x){ return x % 2 == 0; }),
    [](int x){ return x * x; }
);
// 懒惰计算：1-100 中偶数的平方，不创建中间集合
```

---

## 7. Task 异步任务

### 最简 Task 实现

```cpp
template<typename T>
class Task {
public:
    struct promise_type {
        T result_;

        std::suspend_always initial_suspend() { return {}; }  // 懒执行
        std::suspend_always final_suspend() noexcept { return {}; }

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        void return_value(T value) { result_ = std::move(value); }
        void unhandled_exception() { throw; }
    };

    // 让 Task 自身可以被 co_await
    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> continuation) {
        // 保存调用者，执行完后恢复它
        handle_.promise().continuation_ = continuation;
        handle_.resume();  // 启动 Task
    }

    T await_resume() { return handle_.promise().result_; }

private:
    std::coroutine_handle<promise_type> handle_;
};
```

### Lazy Task vs Eager Task

```cpp
// Lazy Task（推荐）：创建时不执行，co_await 时才执行
struct promise_type {
    std::suspend_always initial_suspend() { return {}; }  // 挂起
};

Task<int> lazy = compute();  // 不执行
int result = co_await lazy;  // 现在才执行

// Eager Task：创建时立即执行
struct promise_type {
    std::suspend_never initial_suspend() { return {}; }  // 不挂起
};

Task<int> eager = compute();  // 立即开始执行
```

### sync_wait：在同步代码中等待协程

```cpp
// 用于 main 函数等非协程环境
template<typename T>
T sync_wait(Task<T> task) {
    // 用条件变量模拟同步等待
    std::mutex mutex;
    std::condition_variable cv;
    std::optional<T> result;
    
    // 包装任务：完成后通知
    auto wrapper = [&]() -> Task<void> {
        result = co_await std::move(task);
        cv.notify_one();
    };
    
    wrapper();
    
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&]{ return result.has_value(); });
    
    return std::move(*result);
}

// 使用
int main() {
    int val = sync_wait(compute_async());
    std::cout << val;
}
```

---

## 8. 调度器与线程池

### 最简调度器

```cpp
class Scheduler {
public:
    // 投递协程到调度器
    void schedule(std::coroutine_handle<> h) {
        std::lock_guard<std::mutex> lock(mutex_);
        ready_queue_.push(h);
        cv_.notify_one();
    }

    // 运行直到所有任务完成
    void run() {
        while (true) {
            std::coroutine_handle<> h;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [&]{ return !ready_queue_.empty(); });
                h = ready_queue_.front();
                ready_queue_.pop();
            }
            h.resume();
            if (ready_queue_.empty()) break;
        }
    }

private:
    std::queue<std::coroutine_handle<>> ready_queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
};
```

### 切换线程的 Awaitable

```cpp
// co_await switch_to_thread_pool{pool}
// 之后的代码在线程池中执行
struct SwitchToThreadPool {
    ThreadPool& pool;

    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        pool.submit([h]{ h.resume(); });
    }

    void await_resume() {}
};

Task<void> mixed_context() {
    std::cout << "在主线程: " << std::this_thread::get_id() << "\n";

    co_await SwitchToThreadPool{my_pool};

    std::cout << "在线程池: " << std::this_thread::get_id() << "\n";
}
```

---

## 9. 同步原语

### Channel（通道）

```
发送者协程 ──[send]──► Channel ──[receive]──► 接收者协程
                       (队列)
```

```cpp
// 使用
Channel<int> ch(/*capacity=*/10);

// 发送者
Task<void> producer(Channel<int>& ch) {
    for (int i = 0; i < 5; ++i) {
        co_await ch.send(i);   // 满了会挂起
    }
}

// 接收者
Task<void> consumer(Channel<int>& ch) {
    while (auto value = co_await ch.receive()) {  // 空了会挂起
        std::cout << *value << "\n";
    }
}
```

### AsyncMutex（异步互斥锁）

```cpp
AsyncMutex mutex;
int shared = 0;

Task<void> worker() {
    co_await mutex.lock();     // 获取锁（等待不阻塞线程）
    shared++;                   // 临界区
    mutex.unlock();             // 释放锁
}

// 或使用 RAII
Task<void> worker_raii() {
    auto guard = co_await mutex.scoped_lock();  // 自动释放
    shared++;
}
```

### Semaphore（信号量）

```cpp
// 限制最多 3 个协程同时访问资源
AsyncSemaphore sem(3);

Task<void> request_handler() {
    co_await sem.acquire();   // 减 1，为 0 时挂起
    // ... 处理请求
    sem.release();            // 加 1，唤醒等待者
}
```

### Barrier（屏障）

```cpp
// 等待 N 个协程都到达屏障点
AsyncBarrier barrier(4);

Task<void> worker(int id) {
    // 第一阶段
    do_phase1(id);

    co_await barrier.wait();  // 等所有人都完成第一阶段

    // 第二阶段（所有人同步开始）
    do_phase2(id);
}
```

---

## 10. 高级模式

### Actor 模型

```
每个 Actor 有独立状态和邮箱，通过消息通信，无共享状态

Actor A ──[Message]──► Mailbox B ──► Actor B
Actor B ──[Message]──► Mailbox A ──► Actor A
```

```cpp
class MyActor {
    Task<void> message_loop() {
        while (true) {
            auto msg = co_await mailbox_.receive();
            if (std::holds_alternative<StopMsg>(msg)) break;
            handle(std::get<WorkMsg>(msg));
        }
    }
};
```

**优势**：无锁、无竞争、天然线程安全

### Pipeline 流水线

```
Source → Stage1 → Stage2 → Stage3 → Sink
  生产     过滤     转换     聚合     消费
```

```cpp
// 每个阶段是一个协程，阶段间通过 Channel 通信
Task<void> pipeline() {
    Channel<int> raw, filtered, transformed;

    auto src  = source(raw);                   // 产生数据
    auto flt  = filter(raw, filtered,          // 过滤偶数
                       [](int x){ return x%2==0; });
    auto xfm  = transform(filtered, transformed,
                          [](int x){ return x*x; }); // 平方
    auto sink = consume(transformed);          // 打印

    co_await when_all(src, flt, xfm, sink);
}
```

### Fan-Out / Fan-In

```
              ┌─► Worker 1 ─┐
              │              │
Input ──►  Fan-Out ─► Worker 2 ─► Fan-In ──► Result
              │              │
              └─► Worker 3 ─┘
```

```cpp
Task<void> parallel_process(std::vector<int> data) {
    std::vector<Task<int>> tasks;

    // Fan-Out：分发
    for (int x : data) {
        tasks.push_back(process_item(x));
    }

    // Fan-In：汇总
    auto results = co_await when_all(std::move(tasks));
}
```

### Cancellation Token（取消令牌）

```cpp
// 创建可取消的任务
Task<void> cancellable_task(CancellationToken token) {
    for (int i = 0; i < 100; ++i) {
        if (token.is_cancelled()) {
            std::cout << "任务被取消\n";
            co_return;
        }
        co_await do_step(i);
    }
}

// 取消
auto [token, source] = make_cancellation_pair();
auto task = cancellable_task(token);
source.cancel();  // 发出取消信号
```

---

## 11. 常见陷阱

### ❌ 陷阱1：悬空引用

```cpp
// 错误！task 析构后，协程内部引用了已销毁的局部变量
Task<int> bad() {
    int x = 42;
    co_await some_async_op();  // 挂起后，x 可能失效
    return co_return x;
}

// ✅ 正确：移动到协程帧，或使用 shared_ptr
Task<int> good() {
    auto x = std::make_shared<int>(42);
    co_await some_async_op();
    co_return *x;
}
```

### ❌ 陷阱2：忘记 co_await

```cpp
Task<int> compute();

Task<void> bad() {
    compute();  // ❌ 忘了 co_await！Task 被创建后立即销毁
    co_return;
}

Task<void> good() {
    co_await compute();  // ✅ 等待完成
    co_return;
}
```

### ❌ 陷阱3：协程帧泄漏

```cpp
// 如果协程被挂起后，handle 从未被 resume 或 destroy，就会泄漏
Task<int> leaky_task() {
    co_await forever_pending_awaitable();  // 永远不会恢复
    co_return 42;
}

// ✅ 确保协程要么被恢复，要么被销毁
```

### ❌ 陷阱4：final_suspend 使用 suspend_never

```cpp
// 危险！
struct promise_type {
    // 协程结束后立即销毁协程帧
    // 但此时调用者可能还持有 handle，造成 use-after-free
    std::suspend_never final_suspend() noexcept { return {}; }
};

// ✅ 推荐始终使用 suspend_always，手动 destroy
std::suspend_always final_suspend() noexcept { return {}; }
```

### ❌ 陷阱5：在挂起点跨越 mutex

```cpp
// 错误！mutex 被锁定时挂起，可能死锁
Task<void> bad() {
    std::unique_lock<std::mutex> lock(mutex_);
    co_await async_op();  // ❌ 挂起时持有锁！
}

// ✅ 在挂起点前释放锁，或使用 AsyncMutex
Task<void> good() {
    co_await async_mutex_.lock();  // 异步锁
    // ... 临界区
    async_mutex_.unlock();
    co_await async_op();  // ✅ 无锁挂起
}
```

### ❌ 陷阱6：栈溢出（无对称转移）

```cpp
// 深度嵌套 co_await 没有对称转移会导致栈溢出
Task<int> a() { co_return co_await b(); }
Task<int> b() { co_return co_await c(); }
// ... 1000 层嵌套

// ✅ 使用对称转移（实现 coroutine_handle<> await_suspend）
```

---

## 12. 最佳实践

### 设计原则

1. **用 `suspend_always` 做 initial_suspend**（懒执行，更安全）
2. **用 `suspend_always` 做 final_suspend**（手动 destroy，避免 UAF）
3. **Task 应该是可移动、不可复制的**
4. **异常要么重新抛出，要么存储在 promise 中**

### 实现模板

```cpp
template<typename T>
class Task {
public:
    struct promise_type {
        std::coroutine_handle<> continuation_;  // 恢复点
        std::variant<std::monostate, T, std::exception_ptr> result_;

        auto initial_suspend() { return std::suspend_always{}; }

        auto final_suspend() noexcept {
            struct FinalAwaiter {
                bool await_ready() noexcept { return false; }
                // 对称转移到调用者
                std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<promise_type> h) noexcept {
                    if (h.promise().continuation_)
                        return h.promise().continuation_;
                    return std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            return FinalAwaiter{};
        }

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        void return_value(T value) {
            result_.template emplace<T>(std::move(value));
        }

        void unhandled_exception() {
            result_.template emplace<std::exception_ptr>(
                std::current_exception()
            );
        }
    };

    bool await_ready() { return handle_.done(); }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) {
        handle_.promise().continuation_ = h;
        return handle_;  // 对称转移
    }

    T await_resume() {
        auto& result = handle_.promise().result_;
        if (std::holds_alternative<std::exception_ptr>(result))
            std::rethrow_exception(std::get<std::exception_ptr>(result));
        return std::move(std::get<T>(result));
    }

    // RAII 管理
    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    ~Task() { if (handle_) handle_.destroy(); }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

private:
    explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}
    std::coroutine_handle<promise_type> handle_;
};
```

### 调试技巧

```cpp
// 1. 打印协程地址（每个协程帧有唯一地址）
void await_suspend(std::coroutine_handle<> h) {
    std::cout << "挂起协程 " << h.address() << "\n";
}

// 2. 追踪协程状态
bool is_done = handle.done();
bool is_valid = !!handle;  // 非空句柄

// 3. 统计未完成的协程数量（泄漏检测）
static std::atomic<int> active_coroutines = 0;
promise_type() { ++active_coroutines; }
~promise_type() { --active_coroutines; }
```

---

## 13. 速查表

### Promise 接口一览

| 方法 | 触发时机 | 返回类型 |
|------|---------|---------|
| `initial_suspend()` | 协程刚创建 | Awaitable |
| `final_suspend()` | 协程结束 | Awaitable (noexcept) |
| `get_return_object()` | 创建返回值 | ReturnType |
| `return_void()` | `co_return;` | void |
| `return_value(v)` | `co_return v;` | void |
| `yield_value(v)` | `co_yield v;` | Awaitable |
| `unhandled_exception()` | 异常逃出协程 | void |
| `await_transform(u)` | 每个 `co_await` | Awaitable |

### Awaiter 接口一览

| 方法 | 作用 | 返回类型 |
|------|------|---------|
| `await_ready()` | 是否可以跳过挂起 | bool |
| `await_suspend(h)` | 挂起时执行 | void / bool / coroutine_handle<> |
| `await_resume()` | 恢复时返回值 | T（即 co_await 的结果） |

### coroutine_handle 接口

| 方法 | 说明 |
|------|------|
| `resume()` | 恢复协程执行 |
| `destroy()` | 销毁协程帧 |
| `done()` | 是否已执行到 final_suspend |
| `promise()` | 获取 promise 对象引用 |
| `address()` | 获取协程帧地址（调试用）|
| `from_promise(p)` | 从 promise 获取 handle |
| `from_address(p)` | 从地址获取 handle |
| `noop_coroutine()` | 空操作 handle（用于对称转移终止）|

### 常用 Awaitable

```cpp
co_await std::suspend_always{};   // 无条件挂起
co_await std::suspend_never{};    // 无条件继续
co_await std::noop_coroutine();   // 对称转移终止
```

### 学习路径

```
入门
 └─► 01_basics        理解 Promise / Handle / Awaitable
      └─► 02_generator  掌握 co_yield / 惰性序列
           └─► 03_async   学会 Task<T> / sync_wait / scheduler
                └─► 04_sync_primitives   Channel / Mutex / Semaphore
                     └─► 05_patterns      Actor / Pipeline / Fan-Out
```

---

*最后更新：2026-03-26*
