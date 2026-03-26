// ============================================================
// C++20 Coroutines - Producer-Consumer Pattern
// 生产者-消费者模式：协程版的线程池任务模型
// ============================================================

#include <coroutine>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

// ============================================================
// 任务队列
// ============================================================
class TaskQueue {
public:
    using Task = std::function<void()>;
    
    // 添加任务
    void enqueue(Task task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }
    
    // 取任务（阻塞）
    Task dequeue() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !tasks_.empty() || stop_; });
        
        if (stop_ && tasks_.empty()) {
            return nullptr;
        }
        
        Task task = std::move(tasks_.front());
        tasks_.pop();
        return task;
    }
    
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
    }
    
    bool empty() {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.empty();
    }

private:
    std::queue<Task> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};

// ============================================================
// 工作线程池
// ============================================================
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this, i] {
                std::cout << "[Worker " << i << "] 启动\n";
                while (true) {
                    auto task = queue_.dequeue();
                    if (!task) {
                        std::cout << "[Worker " << i << "] 退出\n";
                        break;
                    }
                    std::cout << "[Worker " << i << "] 执行任务\n";
                    task();
                }
            });
        }
    }
    
    ~ThreadPool() {
        queue_.stop();
        for (auto& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
    }
    
    template<typename F>
    void submit(F&& f) {
        queue_.enqueue(std::forward<F>(f));
    }

private:
    TaskQueue queue_;
    std::vector<std::thread> workers_;
};

// ============================================================
// 协程任务（配合线程池使用）
// ============================================================
template<typename T>
class CoroutineTask {
public:
    struct promise_type {
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        CoroutineTask get_return_object() {
            return CoroutineTask::from_promise(this);
        }
        
        void return_value(T value) {
            result_ = std::make_shared<T>(std::move(value));
            ready_ = true;
        }
        
        void unhandled_exception() {
            exception_ = std::current_exception();
        }
        
        std::shared_ptr<T> result_;
        std::exception_ptr exception_;
        std::atomic<bool> ready_{false};
    };

    bool await_ready() { return promise_->ready_; }
    
    void await_suspend(std::coroutine_handle<>) {}
    
    T await_resume() {
        if (promise_->exception_) {
            std::rethrow_exception(promise_->exception_);
        }
        return *promise_->result_;
    }

private:
    explicit CoroutineTask(promise_type* p) : promise_(p) {}
    static CoroutineTask from_promise(promise_type* p) { return CoroutineTask(p); }
    
    promise_type* promise_;
};

// ============================================================
// 生产者协程
// ============================================================
struct ProducerTask {
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<>) {}
    void await_resume() {}
};

ProducerTask producer(TaskQueue& queue, int id, int count) {
    for (int i = 0; i < count; ++i) {
        int value = id * 100 + i;
        std::cout << "[Producer " << id << "] 生产任务: " << value << "\n";
        
        queue.enqueue([value]() {
            std::cout << "[Task " << value << "] 被执行\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        });
        
        co_await std::suspend_always{};
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

// ============================================================
// 消费者协程
// ============================================================
ProducerTask consumer(TaskQueue& queue, int id) {
    std::cout << "[Consumer " << id << "] 启动\n";
    
    for (int i = 0; i < 3; ++i) {
        // 模拟等待
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "[Consumer " << id << "] 第 " << (i+1) << " 轮检查\n";
    }
}

// ============================================================
// 带优先级的任务队列
// ============================================================
class PriorityTaskQueue {
public:
    using Task = std::function<void()>;
    
    struct PrioritizedTask {
        int priority;
        int sequence;
        Task task;
        
        bool operator<(const PrioritizedTask& other) const {
            return priority > other.priority; // 越小优先级越高
        }
    };
    
    void enqueue(Task task, int priority = 0) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.emplace(PrioritizedTask{priority, seq_++, std::move(task)});
        }
        cv_.notify_one();
    }
    
    Task dequeue() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !tasks_.empty() || stop_; });
        
        if (stop_ && tasks_.empty()) return nullptr;
        
        Task task = std::move(const_cast<Task&>(tasks_.top().task));
        tasks_.pop();
        return task;
    }
    
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
    }

private:
    std::priority_queue<PrioritizedTask> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    int seq_ = 0;
    bool stop_ = false;
};

// ============================================================
// 演示
// ============================================================
int main() {
    std::cout << "=== Producer-Consumer Pattern Demo ===\n\n";
    
    // 创建工作线程池
    ThreadPool pool(3);
    
    std::cout << "线程池已创建，3 个工作线程\n\n";
    
    // 提交一些任务
    std::cout << "--- 提交任务到线程池 ---\n";
    pool.submit([] {
        std::cout << "[Pool Task] 任务 A 开始\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "[Pool Task] 任务 A 完成\n";
    });
    
    pool.submit([] {
        std::cout << "[Pool Task] 任务 B 开始\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::cout << "[Pool Task] 任务 B 完成\n";
    });
    
    pool.submit([] {
        std::cout << "[Pool Task] 任务 C 开始\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        std::cout << "[Pool Task] 任务 C 完成\n";
    });
    
    // 使用任务队列
    std::cout << "\n--- 使用任务队列 ---\n";
    TaskQueue task_queue;
    
    // 模拟生产者-消费者
    std::thread producer_thread([&]() {
        for (int i = 0; i < 5; ++i) {
            int value = i + 1;
            std::cout << "[Producer] 提交任务 #" << value << "\n";
            task_queue.enqueue([value]() {
                std::cout << "[Queue Task #" << value << "] 执行中...\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        std::cout << "[Producer] 生产完成\n";
    });
    
    std::thread consumer_thread([&]() {
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            std::cout << "[Consumer] 尝试获取任务...\n";
            auto task = task_queue.dequeue();
            if (task) {
                task();
            }
        }
        std::cout << "[Consumer] 消费完成\n";
    });
    
    producer_thread.join();
    consumer_thread.join();
    
    std::cout << "\n=== 生产者-消费者演示完成 ===\n";
    
    return 0;
}
