// ============================================================
// C++20 Coroutines - Pipeline Pattern
// 流水线模式：数据像水流一样流经一系列处理阶段
// ============================================================

#include <coroutine>
#include <vector>
#include <queue>
#include <functional>
#include <iostream>
#include <thread>
#include <chrono>
#include <optional>

// ============================================================
// 流水线阶段基类
// ============================================================
template<typename Input, typename Output>
class PipelineStage {
public:
    virtual ~PipelineStage() = default;
    virtual Output process(Input input) = 0;
};

// ============================================================
// 异步流水线
// ============================================================
template<typename T>
class AsyncPipeline {
public:
    struct OutputAwaitable {
        AsyncPipeline& pipeline;
        
        bool await_ready() {
            return !pipeline.output_queue_.empty();
        }
        
        void await_suspend(std::coroutine_handle<> h) {
            pipeline.waiting_receivers_.push(h);
        }
        
        std::optional<T> await_resume() {
            if (pipeline.output_queue_.empty()) {
                return std::nullopt;
            }
            T value = std::move(pipeline.output_queue_.front());
            pipeline.output_queue_.pop();
            return value;
        }
    };

    OutputAwaitable output() { return {*this}; }
    
    void push(T value) {
        output_queue_.push(std::move(value));
        if (!waiting_receivers_.empty()) {
            waiting_receivers_.front().resume();
            waiting_receivers_.pop();
        }
    }

private:
    std::queue<T> output_queue_;
    std::queue<std::coroutine_handle<>> waiting_receivers_;
};

// ============================================================
// 阶段处理器
// ============================================================
template<typename T>
class StageHandler {
public:
    virtual ~StageHandler() = default;
    virtual T handle(T input) = 0;
};

// ============================================================
// 协程流水线
// ============================================================
class CoroutinePipeline {
public:
    struct Task {
        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<>) {}
        void await_resume() {}
    };

    // 阶段1: 数据源
    template<typename Iterable>
    Task source(Iterable&& data) {
        for (auto&& item : data) {
            co_await stage1().input().send(item);
        }
        stage1().input().send({}); // 发送结束信号
    }

    // 阶段2: 过滤器
    template<typename Predicate>
    Task filter(Predicate pred) {
        while (true) {
            auto item = co_await stage1().input().receive();
            if (!item.has_value()) break;
            if (pred(*item)) {
                co_await stage2().input().send(*item);
            }
        }
        stage2().input().send({});
    }

    // 阶段3: 转换
    template<typename Transform>
    Task transform(Transform fn) {
        while (true) {
            auto item = co_await stage2().input().receive();
            if (!item.has_value()) break;
            co_await sink().input().send(fn(*item));
        }
        sink().input().send({});
    }

    // 阶段4: 消费者
    Task consume(std::function<void(int)> handler) {
        while (true) {
            auto item = co_await sink().input().receive();
            if (!item.has_value()) break;
            handler(*item);
        }
    }

    // 获取各阶段（简化）
    auto& stage1() { return *this; }
    auto& stage2() { return *this; }
    auto& sink() { return *this; }

private:
    AsyncPipeline<int> pipeline1_, pipeline2_, pipeline3_;
};

// ============================================================
// 简单版流水线演示
// ============================================================
class SimplePipeline {
public:
    struct PipelineTask {
        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<>) {}
        void await_resume() {}
    };

    // 生产者阶段
    PipelineTask producer(const std::vector<int>& data) {
        std::cout << "[Stage 1] 生产者: 开始生成数据\n";
        for (int value : data) {
            std::cout << "[Stage 1] 生成: " << value << "\n";
            data_queue_.push(value);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        std::cout << "[Stage 1] 生产者: 完成\n";
        finished_ = true;
    }

    // 过滤阶段（只保留偶数）
    PipelineTask filter() {
        std::cout << "[Stage 2] 过滤器: 等待数据...\n";
        while (!finished_ || !data_queue_.empty()) {
            if (!data_queue_.empty()) {
                int value = data_queue_.front();
                data_queue_.pop();
                
                if (value % 2 == 0) {
                    std::cout << "[Stage 2] 过滤后保留: " << value << "\n";
                    filtered_queue_.push(value);
                } else {
                    std::cout << "[Stage 2] 过滤掉: " << value << "\n";
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::cout << "[Stage 2] 过滤器: 完成\n";
        filter_finished_ = true;
    }

    // 转换阶段（乘以2）
    PipelineTask transformer() {
        std::cout << "[Stage 3] 转换器: 等待数据...\n";
        while (!filter_finished_ || !filtered_queue_.empty()) {
            if (!filtered_queue_.empty()) {
                int value = filtered_queue_.front();
                filtered_queue_.pop();
                int transformed = value * 2;
                std::cout << "[Stage 3] 转换 " << value << " -> " << transformed << "\n";
                output_queue_.push(transformed);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::cout << "[Stage 3] 转换器: 完成\n";
    }

    // 消费者阶段
    PipelineTask consumer() {
        std::cout << "[Stage 4] 消费者: 等待数据...\n";
        while (!transformer_finished_ || !output_queue_.empty()) {
            if (!output_queue_.empty()) {
                int value = output_queue_.front();
                output_queue_.pop();
                std::cout << "[Stage 4] 消费者收到: " << value << "\n";
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::cout << "[Stage 4] 消费者: 完成\n";
    }

    void set_transformer_finished() { transformer_finished_ = true; }

private:
    std::queue<int> data_queue_;
    std::queue<int> filtered_queue_;
    std::queue<int> output_queue_;
    std::atomic<bool> finished_{false};
    std::atomic<bool> filter_finished_{false};
    std::atomic<bool> transformer_finished_{false};
};

// ============================================================
// 演示
// ============================================================
int main() {
    std::cout << "=== Pipeline Pattern Demo ===\n\n";
    
    SimplePipeline pipeline;
    
    // 启动流水线各阶段
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    std::thread t1([&]() { pipeline.producer(data); });
    std::thread t2([&]() { pipeline.filter(); });
    std::thread t3([&]() { 
        pipeline.transformer(); 
        pipeline.set_transformer_finished();
    });
    std::thread t4([&]() { pipeline.consumer(); });
    
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    
    std::cout << "\n=== 流水线执行完成 ===\n";
    std::cout << "流程: 生产 -> 过滤(偶数) -> 转换(*2) -> 消费\n";
    std::cout << "输入: 1-10 的整数\n";
    std::cout << "输出: 偶数 * 2 = 4, 8, 12, 16, 20\n";
    
    return 0;
}
