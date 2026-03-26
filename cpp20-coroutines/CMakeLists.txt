cmake_minimum_required(VERSION 3.20)
project(cpp20_coroutines CXX)

# C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: 启用协程 (/await)
if(MSVC)
    add_compile_options(/await)
endif()

# GCC/Clang: 启用协程 (-fcoroutines)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fcoroutines)
endif()

# ============================================================
# 01_basics - 基础协程
# ============================================================
add_executable(basics_01_minimal 01_basics/01_minimal_coroutine.cpp)
add_executable(basics_02_suspend 01_basics/02_suspend_types.cpp)
add_executable(basics_03_awaitable 01_basics/03_custom_awaitable.cpp)

# ============================================================
# 02_generator - 生成器
# ============================================================
add_executable(gen_01_basic 02_generator/01_generator.cpp)
add_executable(gen_02_fibonacci 02_generator/02_fibonacci.cpp)

# ============================================================
# 03_async - 异步任务
# ============================================================
add_executable(async_01_task 03_async/01_task.cpp)
add_executable(async_02_lazy 03_async/02_lazy_task.cpp)
add_executable(async_03_sync_wait 03_async/03_sync_wait.cpp)
add_executable(async_04_scheduler 03_async/04_custom_scheduler.cpp)
add_executable(async_05_timer 03_async/05_timer_awaitable.cpp)
add_executable(async_06_io 03_async/06_async_io.cpp)
add_executable(async_07_continuation 03_async/07_continuation.cpp)
add_executable(async_08_parallel 03_async/08_parallel_await.cpp)

# ============================================================
# 04_sync_primitives - 同步原语
# ============================================================
add_executable(sync_01_channel 04_sync_primitives/01_channel.cpp)
add_executable(sync_02_mutex 04_sync_primitives/02_mutex.cpp)
add_executable(sync_03_semaphore 04_sync_primitives/03_semaphore.cpp)

# ============================================================
# 05_patterns - 高级模式
# ============================================================
add_executable(pattern_01_actor 05_patterns/01_actor.cpp)
add_executable(pattern_02_pipeline 05_patterns/02_pipeline.cpp)
add_executable(pattern_03_producer_consumer 05_patterns/03_producer_consumer.cpp)
add_executable(pattern_04_fan_out_in 05_patterns/04_fan_out_in.cpp)
add_executable(pattern_05_gen_pipeline 05_patterns/05_generator_pipeline.cpp)
add_executable(pattern_06_cancel_timeout 05_patterns/06_cancellation_timeout.cpp)

# ============================================================
# 构建所有示例
# ============================================================
add_custom_target(all_examples
    basics_01_minimal basics_02_suspend basics_03_awaitable
    gen_01_basic gen_02_fibonacci
    async_01_task async_02_lazy async_03_sync_wait
    async_04_scheduler async_05_timer async_06_io
    async_07_continuation async_08_parallel
    sync_01_channel sync_02_mutex sync_03_semaphore
    pattern_01_actor pattern_02_pipeline pattern_03_producer_consumer
    pattern_04_fan_out_in pattern_05_gen_pipeline pattern_06_cancel_timeout
    COMMENT "Building all C++20 coroutine examples"
)
