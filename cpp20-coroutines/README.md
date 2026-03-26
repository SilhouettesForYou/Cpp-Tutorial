# C++20 Coroutines 示例代码

深入学习 C++20 协程的完整示例代码库。

## 📁 目录结构

```
cpp20-coroutines/
├── 01_basics/          # 基础概念
│   ├── 01_minimal_coroutine.cpp
│   ├── 02_suspend_types.cpp
│   └── 03_custom_awaitable.cpp
│
├── 02_generator/       # 生成器
│   ├── 01_generator.cpp
│   └── 02_fibonacci.cpp
│
├── 03_async/           # 异步任务
│   ├── 01_task.cpp
│   ├── 02_lazy_task.cpp
│   ├── 03_sync_wait.cpp
│   ├── 04_custom_scheduler.cpp
│   ├── 05_timer_awaitable.cpp
│   ├── 06_async_io.cpp
│   ├── 07_continuation.cpp
│   └── 08_parallel_await.cpp
│
├── 04_sync_primitives/ # 同步原语
│   ├── 01_channel.cpp
│   ├── 02_mutex.cpp
│   └── 03_semaphore.cpp
│
├── 05_patterns/        # 高级模式
│   ├── 01_actor.cpp
│   ├── 02_pipeline.cpp
│   ├── 03_producer_consumer.cpp
│   ├── 04_fan_out_in.cpp
│   ├── 05_generator_pipeline.cpp
│   └── 06_cancellation_timeout.cpp
│
├── CMakeLists.txt
└── README.md
```

## 🔨 编译

### 前置要求
- **MSVC** 19.28+ (Visual Studio 2019 16.8+)
- **GCC** 10+ (需 libstdc++)
- **Clang** 11+ (需 libc++)

### 构建所有示例
```bash
mkdir build && cd build
cmake ..
cmake --build . --target all_examples
```

### 构建单个示例
```bash
# Linux/macOS
g++ -std=c++20 -fcoroutines -o example example.cpp -lpthread

# Windows (MSVC)
cl /std:c++20 /await /EHsc example.cpp
```

## 📚 内容概览

### 01_basics - 基础概念
| 文件 | 描述 |
|------|------|
| `01_minimal_coroutine.cpp` | 最小协程骨架 |
| `02_suspend_types.cpp` | suspend_never vs suspend_always |
| `03_custom_awaitable.cpp` | 自定义 Awaitable |

### 02_generator - 生成器
| 文件 | 描述 |
|------|------|
| `01_generator.cpp` | Generator<T> 实现 |
| `02_fibonacci.cpp` | 无限斐波那契序列 |

### 03_async - 异步任务
| 文件 | 描述 |
|------|------|
| `01_task.cpp` | Task<T> 最简异步任务 |
| `02_lazy_task.cpp` | 惰性执行任务 |
| `03_sync_wait.cpp` | 同步等待异步任务 |
| `04_custom_scheduler.cpp` | 自定义线程池调度器 |
| `05_timer_awaitable.cpp` | 定时器 Awaitable |
| `06_async_io.cpp` | 模拟异步 I/O |
| `07_continuation.cpp` | 协程链接（then 模式）|
| `08_parallel_await.cpp` | 并行等待多个协程 |

### 04_sync_primitives - 同步原语
| 文件 | 描述 |
|------|------|
| `01_channel.cpp` | Channel（Go-style 通道）|
| `02_mutex.cpp` | AsyncMutex 异步互斥锁 |
| `03_semaphore.cpp` | Semaphore & Barrier 信号量与屏障 |

### 05_patterns - 高级模式
| 文件 | 描述 |
|------|------|
| `01_actor.cpp` | Actor 模型 |
| `02_pipeline.cpp` | Pipeline 流水线模式 |
| `03_producer_consumer.cpp` | 生产者-消费者模式 |
| `04_fan_out_in.cpp` | Fan-Out / Fan-In 模式 |
| `05_generator_pipeline.cpp` | 生成器流水线（map/filter）|
| `06_cancellation_timeout.cpp` | 取消与超时控制 |

## ⚙️ CMake 预设目标

| 目标 | 说明 |
|------|------|
| `all_examples` | 构建所有示例 |
| `basics_01_minimal` | 最小协程 |
| `gen_01_basic` | 基础生成器 |
| `async_01_task` | 异步任务 |
| `sync_01_channel` | 通道 |
| `pattern_01_actor` | Actor 模型 |

## 📖 学习路径

1. **入门**: 先读 `01_basics`，理解 Promise、Handle、Awaitable 三大核心
2. **进阶**: 看 `02_generator`，掌握 co_yield 和惰性迭代
3. **实战**: 深入 `03_async`，学会构建实用的异步系统
4. **并发**: 学习 `04_sync_primitives`，掌握协程同步原语
5. **架构**: 掌握 `05_patterns`，构建复杂的协程应用

## 🔗 相关资源

- [C++ 标准提案 P0057](https://wg21.link/P0057)
- [cppreference 协程](https://en.cppreference.com/w/cpp/language/coroutines)
- [C++20 协程详解](https://github.com/GavinClarke/CppCoroutines)

---
*祝学习愉快！* 🚀
