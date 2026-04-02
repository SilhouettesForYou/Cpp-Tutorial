cmake_minimum_required(VERSION 3.20)
project(CppMemoryModelTutorial)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable UTF-8 encoding
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/utf-8)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# ==================== 01_basics ====================
add_executable(demo_01_atomic_basic 01_basics/01_atomic_basic.cpp)

# ==================== 02_atomic ====================
add_executable(demo_02_atomic_ops 02_atomic/01_atomic_ops.cpp)
add_executable(demo_02_atomic_flag 02_atomic/02_atomic_flag.cpp)

# ==================== 03_mo ====================
add_executable(demo_03_memory_order 03_mo/01_memory_order.cpp)
add_executable(demo_03_producer_consumer 03_mo/02_producer_consumer.cpp)

# ==================== 04_fences ====================
add_executable(demo_04_fence_basics 04_fences/01_fence_basics.cpp)

# ==================== 05_lockfree ====================
add_executable(demo_05_cas 05_lockfree/01_cas.cpp)
add_executable(demo_05_lockfree_stack 05_lockfree/02_lockfree_stack.cpp)
add_executable(demo_05_aba_problem 05_lockfree/03_aba_problem.cpp)
