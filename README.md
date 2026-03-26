# C++ Tutorial

C++ 学习教程集合，包含多个主题。

## 目录结构

```
cpp-tutorial/
├── cpp20-coroutines/     # C++20 协程
│   ├── 01_basics/         # 基础协程
│   ├── 02_generator/      # 生成器
│   ├── 03_async/          # 异步任务
│   ├── 04_sync_primitives/ # 同步原语
│   ├── 05_patterns/      # 高级模式
│   ├── CMakeLists.txt
│   ├── README.md
│   └── NOTES.md
│
└── memory-model/         # C++ 内存模型
    ├── 01_basics/        # 基础概念
    ├── 02_atomic/        # 原子操作
    ├── 03_mo/            # 内存顺序
    ├── 04_fences/        # 内存屏障
    ├── 05_lockfree/      # 无锁编程
    └── NOTES.md
```

## 编译

```bash
# 编译协程示例
cd cpp20-coroutines
mkdir build && cd build
cmake .. -DCMAKE_CXX_STANDARD=20
cmake --build .

# 编译内存模型示例
cd ../memory-model
mkdir build && cd build
cmake .. -DCMAKE_CXX_STANDARD=20
cmake --build .
```

## 贡献

欢迎提交 Issue 和 Pull Request！
