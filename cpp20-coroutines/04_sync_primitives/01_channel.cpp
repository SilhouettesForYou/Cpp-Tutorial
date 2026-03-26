// ============================================================
// C++20 Coroutines - Channel (Go-style channels)
// 通道：线程安全的双向通信机制
// ============================================================

#include <coroutine>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <iostream>
#include <thread>
#include <chrono>

template<typename T>
class Channel {
public:
    // 发送者端
    class Sender {
    public:
        Sender(Channel* channel) : channel_(channel) {}
        Sender(Sender&& other) noexcept : channel_(other.channel_) {
            other.channel_ = nullptr;
        }
        ~Sender() {
            // 关闭时通知所有等待的接收者
            if (channel_) {
                std::lock_guard<std::mutex> lock(channel_->mutex_);
                channel_->closed_ = true;
                channel_->cv_.notify_all();
            }
        }

        // 发送操作（协程中等待）
        bool send(T value) {
            if (!channel_) return false;
            
            std::unique_lock<std::mutex> lock(channel_->mutex_);
            
            // 等待直到有空间或通道关闭
            channel_->cv_.wait(lock, [this] {
                return channel_->queue_.size() < channel_->capacity_ 
                       || channel_->closed_;
            });
            
            if (channel_->closed_) return false;
            
            channel_->queue_.push(std::move(value));
            channel_->cv_.notify_one();
            return true;
        }

        // 协程中使用的 send 等待点
        struct SendAwaitable {
            Sender& sender;
            T value;
            
            bool await_ready() { return false; }
            
            void await_suspend(std::coroutine_handle<> h) {
                // 存储协程句柄，等待可以发送时恢复
                sender.channel_->waiting_senders_.push(h);
                // 尝试立即发送
                if (sender.send(std::move(value))) {
                    // 发送成功，恢复协程
                    h.resume();
                }
            }
            
            void await_resume() {}
        };

        SendAwaitable send_async(T value) {
            return {*this, std::move(value)};
        }

    private:
        Channel* channel_;
    };

    // 接收者端
    class Receiver {
    public:
        Receiver(Channel* channel) : channel_(channel) {}
        Receiver(Receiver&& other) noexcept : channel_(other.channel_) {
            other.channel_ = nullptr;
        }

        // 接收操作（协程中等待）
        std::optional<T> receive() {
            if (!channel_) return std::nullopt;
            
            std::unique_lock<std::mutex> lock(channel_->mutex_);
            
            // 等待直到有数据或通道关闭
            channel_->cv_.wait(lock, [this] {
                return !channel_->queue_.empty() || channel_->closed_;
            });
            
            if (channel_->queue_.empty()) return std::nullopt; // 已关闭
            
            T value = std::move(channel_->queue_.front());
            channel_->queue_.pop();
            channel_->cv_.notify_one(); // 通知等待的发送者
            return value;
        }

        // 协程中使用的 receive 等待点
        struct [[nodiscard]] ReceiveAwaitable {
            Receiver& receiver;
            
            bool await_ready() { return false; }
            
            void await_suspend(std::coroutine_handle<> h) {
                receiver.channel_->waiting_receivers_.push(h);
                // 尝试立即接收
                if (auto value = receiver.receive()) {
                    // 接收成功，恢复协程
                    h.resume();
                }
            }
            
            std::optional<T> await_resume() {
                return receiver.receive();
            }
        };

        ReceiveAwaitable receive_async() { return {*this}; }

    private:
        Channel* channel_;
    };

    Channel(size_t capacity = 0) : capacity_(capacity) {}

    Sender get_sender() { return Sender(this); }
    Receiver get_receiver() { return Receiver(this); }

private:
    friend class Sender;
    friend class Receiver;
    
    std::queue<T> queue_;
    size_t capacity_; // 0 表示无界
    bool closed_ = false;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::coroutine_handle<>> waiting_senders_;
    std::queue<std::coroutine_handle<>> waiting_receivers_;
};

// ============================================================
// 简化版本：使用 std::promise/future 配合协程
// ============================================================

template<typename T>
class SimpleChannel {
public:
    struct SendOperation {
        SimpleChannel& ch;
        T value;
        
        bool await_ready() { 
            return ch.closed_; 
        }
        
        void await_suspend(std::coroutine_handle<> h) {
            std::lock_guard<std::mutex> lock(ch.mutex_);
            if (ch.closed_) {
                h.resume();
                return;
            }
            ch.sender_queue_.push({std::move(value), h});
        }
        
        bool await_resume() { 
            return !ch.closed_; 
        }
    };

    struct ReceiveOperation {
        SimpleChannel& ch;
        
        bool await_ready() { 
            return ch.closed_ || !ch.queue_.empty(); 
        }
        
        void await_suspend(std::coroutine_handle<> h) {
            std::lock_guard<std::mutex> lock(ch.mutex_);
            if (ch.closed_ || !ch.queue_.empty()) {
                h.resume();
                return;
            }
            ch.receiver_queue_.push(h);
        }
        
        T await_resume() {
            std::lock_guard<std::mutex> lock(ch.mutex_);
            if (!ch.queue_.empty()) {
                T val = std::move(ch.queue_.front());
                ch.queue_.pop();
                return val;
            }
            return T{}; // 通道已关闭
        }
    };

    SendOperation send(T value) { return {*this, std::move(value)}; }
    ReceiveOperation receive() { return {*this}; }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        // 恢复所有等待的协程
        while (!receiver_queue_.empty()) {
            receiver_queue_.front().resume();
            receiver_queue_.pop();
        }
    }

private:
    friend struct SendOperation;
    friend struct ReceiveOperation;
    
    std::queue<T> queue_;
    std::queue<std::coroutine_handle<>> receiver_queue_;
    std::queue<std::pair<T, std::coroutine_handle<>>> sender_queue_;
    bool closed_ = false;
    std::mutex mutex_;
};

// ============================================================
// 演示：使用 Channel 进行生产者-消费者通信
// ============================================================

struct ProducerConsumerTask {
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<>) {}
    void await_resume() {}
};

// 生产者协程
ProducerConsumerTask producer(SimpleChannel<int>& ch, int id, int count) {
    for (int i = 0; i < count; ++i) {
        int value = id * 100 + i;
        std::cout << "[Producer " << id << "] 发送: " << value << "\n";
        co_await ch.send(value);
        
        // 模拟生产时间
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::cout << "[Producer " << id << "] 完成\n";
}

// 消费者协程
ProducerConsumerTask consumer(SimpleChannel<int>& ch, int id) {
    while (true) {
        int value = co_await ch.receive();
        if (value == -1) break; // 结束信号
        std::cout << "[Consumer " << id << "] 收到: " << value << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    std::cout << "[Consumer " << id << "] 完成\n";
}

int main() {
    std::cout << "=== C++20 Coroutine Channel Demo ===\n\n";
    
    SimpleChannel<int> ch;
    
    // 启动多个生产者
    std::cout << "启动 2 个生产者，3 个消费者...\n\n";
    
    producer(ch, 1, 5);
    producer(ch, 2, 5);
    consumer(ch, 1);
    consumer(ch, 2);
    consumer(ch, 3);
    
    // 发送结束信号
    ch.close();
    
    // 等待所有协程完成
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "\n=== 通道通信完成 ===\n";
    
    return 0;
}
