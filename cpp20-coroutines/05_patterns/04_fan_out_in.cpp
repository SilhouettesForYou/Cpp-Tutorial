// ============================================================
// C++20 Coroutines - Fan-Out / Fan-In Pattern
// 扇出/扇入模式：分发给多个协程并行处理，结果汇总
// ============================================================

#include <coroutine>
#include <vector>
#include <queue>
#include <mutex>
#include <future>
#include <functional>
#include <iostream>
#include <thread>
#include <chrono>

// ============================================================
// 结果收集器
// ============================================================
template<typename T>
class ResultCollector {
public:
    void add(T result) {
        std::lock_guard<std::mutex> lock(mutex_);
        results_.push_back(std::move(result));
    }
    
    std::vector<T> get_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::move(results_);
    }
    
    size_t count() {
        std::lock_guard<std::mutex> lock(mutex_);
        return results_.size();
    }

private:
    std::vector<T> results_;
    std::mutex mutex_;
};

// ============================================================
// 扇出：分发给多个工作协程
// ============================================================
template<typename T, typename Func>
std::vector<std::coroutine_handle<>> fan_out(
    std::vector<T> items,
    Func worker_func,
    ResultCollector<typename std::invoke_result_t<Func, T>::element_type>& results
) {
    std::vector<std::coroutine_handle<>> handles;
    
    for (T item : items) {
        auto task = worker_func(std::move(item), results);
        handles.push_back(std::coroutine_handle<>::from_promise(*task.promise_));
    }
    
    return handles;
}

// ============================================================
// 简化版的扇出协程
// ============================================================
class FanOutDemo {
public:
    struct WorkTask {
        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<>) {}
        void await_resume() {}
    };
    
    template<typename T>
    WorkTask worker(int id, T data, ResultCollector<int>& results) {
        std::cout << "[Worker " << id << "] 开始处理: " << data << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100 + id * 20));
        int result = data * 2;
        results.add(result);
        std::cout << "[Worker " << id << "] 完成: " << data << " -> " << result << "\n";
        co_return;
    }
};

// ============================================================
// 并行任务执行器
// ============================================================
template<typename T>
class ParallelExecutor {
public:
    struct TaskResult {
        int worker_id;
        T input;
        T output;
        bool completed;
    };
    
    std::vector<TaskResult> execute(
        std::vector<std::pair<int, T>> tasks,
        std::function<T(int, T)> processor
    ) {
        std::mutex result_mutex;
        std::vector<TaskResult> all_results;
        std::atomic<int> completed_count{0};
        
        std::vector<std::thread> workers;
        
        for (auto& [id, data] : tasks) {
            workers.emplace_back([id, data, &processor, &result_mutex, 
                                 &all_results, &completed_count]() {
                T result = processor(id, data);
                
                std::lock_guard<std::mutex> lock(result_mutex);
                all_results.push_back({id, data, result, true});
                ++completed_count;
            });
        }
        
        for (auto& w : workers) {
            w.join();
        }
        
        return all_results;
    }
};

// ============================================================
// 演示
// ============================================================
int main() {
    std::cout << "=== Fan-Out / Fan-In Pattern Demo ===\n\n";
    
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8};
    
    std::cout << "输入数据: ";
    for (int d : data) std::cout << d << " ";
    std::cout << "\n\n";
    
    ResultCollector<int> results;
    
    auto processor = [](int id, int data) -> int {
        std::cout << "[Worker " << id << "] 处理 " << data << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return data * 2;
    };
    
    std::cout << "--- Fan-Out: 分发给 8 个工作线程 ---\n";
    ParallelExecutor<int> executor;
    
    std::vector<std::pair<int, int>> tasks;
    for (size_t i = 0; i < data.size(); ++i) {
        tasks.push_back({static_cast<int>(i), data[i]});
    }
    
    auto all_results = executor.execute(tasks, processor);
    
    std::cout << "\n--- Fan-In: 收集所有结果 ---\n";
    std::cout << "完成的工作数: " << all_results.size() << "\n";
    std::cout << "结果: ";
    for (auto& r : all_results) {
        std::cout << r.output << " ";
    }
    std::cout << "\n";
    
    std::cout << "\n=== Fan-Out / Fan-In 演示完成 ===\n";
    
    return 0;
}
