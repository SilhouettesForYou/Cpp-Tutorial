cmake_minimum_required(VERSION 3.20)
project(CppMemoryModelTutorial)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==================== 01_basics ====================
add_executable(demo_01_atomic_basic 01_basics/01_atomic_basic.cpp)
target_compile_options(demo_01_atomic_basic PRIVATE -Wall -Wextra)

# ==================== 02_atomic ====================
add_executable(demo_02_atomic_ops 02_atomic/01_atomic_ops.cpp)
target_compile_options(demo_02_atomic_ops PRIVATE -Wall -Wextra)

add_executable(demo_02_atomic_flag 02_atomic/02_atomic_flag.cpp)
target_compile_options(demo_02_atomic_flag PRIVATE -Wall -Wextra)

# ==================== 03_mo ====================
add_executable(demo_03_memory_order 03_mo/01_memory_order.cpp)
target_compile_options(demo_03_memory_order PRIVATE -Wall -Wextra)

add_executable(demo_03_producer_consumer 03_mo/02_producer_consumer.cpp)
target_compile_options(demo_03_producer_consumer PRIVATE -Wall -Wextra)

# ==================== 04_fences ====================
add_executable(demo_04_fence_basics 04_fences/01_fence_basics.cpp)
target_compile_options(demo_04_fence_basics PRIVATE -Wall -Wextra)

# ==================== 05_lockfree ====================
add_executable(demo_05_cas 05_lockfree/01_cas.cpp)
target_compile_options(demo_05_cas PRIVATE -Wall -Wextra)

add_executable(demo_05_lockfree_stack 05_lockfree/02_lockfree_stack.cpp)
target_compile_options(demo_05_lockfree_stack PRIVATE -Wall -Wextra)

add_executable(demo_05_aba_problem 05_lockfree/03_aba_problem.cpp)
target_compile_options(demo_05_aba_problem PRIVATE -Wall -Wextra)

# ==================== Colorful output ====================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    foreach(target demo_01_atomic_basic demo_02_atomic_ops demo_02_atomic_flag
                     demo_03_memory_order demo_03_producer_consumer
                     demo_04_fence_basics
                     demo_05_cas demo_05_lockfree_stack demo_05_aba_problem)
        target_compile_options(${target} PRIVATE -fdiagnostics-color=always)
    endforeach()
endif()
