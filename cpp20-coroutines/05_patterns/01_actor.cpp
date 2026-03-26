// ============================================================
// C++20 Coroutines - Actor Model
// Actor 模型：每个 Actor 有独立的邮箱和状态，通过消息通信
// ============================================================

#include <coroutine>
#include <queue>
#include <mutex>
#include <memory>
#include <variant>
#include <optional>
#include <iostream>
#include <functional>
#include <thread>
#include <chrono>

// ============================================================
// 消息类型定义
// ============================================================
struct PingMessage { int from_id; };
struct PongMessage { int from_id; };
struct StopMessage {};
struct PrintMessage { std::string text; };

using Message = std::variant<PingMessage, PongMessage, StopMessage, PrintMessage>;

// ============================================================
// Actor 基类
// ============================================================
class Actor {
public:
    Actor(int id) : id_(id) {}
    virtual ~Actor() = default;
    
    int id() const { return id_; }
    
    // 发送消息给 Actor
    void send(Message msg) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            mailbox_.push(std::move(msg));
        }
        notify();
    }
    
    // 处理所有待处理消息
    void process_messages() {
        while (true) {
            Message msg;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (mailbox_.empty()) return;
                msg = std::move(mailbox_.front());
                mailbox_.pop();
            }
            
            if (std::holds_alternative<StopMessage>(msg)) {
                break;
            }
            
            handle_message(std::move(msg));
        }
    }
    
protected:
    virtual void handle_message(Message msg) = 0;
    
    int id_;
    std::mutex mutex_;
    std::queue<Message> mailbox_;
    std::condition_variable cv_;
    bool notified_ = false;
    
private:
    void notify() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            notified_ = true;
        }
        cv_.notify_one();
    }
};

// ============================================================
// Ping Actor
// ============================================================
class PingActor : public Actor {
public:
    PingActor(int id, std::shared_ptr<Actor> partner, int count)
        : Actor(id), partner_(partner), count_(count), sent_(0) {}
    
    void start() {
        // 发送初始消息
        partner_->send(PingMessage{id_});
        ++sent_;
    }

protected:
    void handle_message(Message msg) override {
        if (std::holds_alternative<PingMessage>(msg)) {
            std::cout << "[Ping" << id_ << "] 收到 Ping!\n";
            
            if (sent_ < count_) {
                // 回复 ping 并发送新的
                partner_->send(PingMessage{id_});
                ++sent_;
            } else if (sent_ == count_) {
                std::cout << "[Ping" << id_ << "] 发送完成，通知停止\n";
                partner_->send(StopMessage{});
                partner_->send(StopMessage{}); // 确保停止
            }
        }
    }

private:
    std::shared_ptr<Actor> partner_;
    int count_;
    int sent_;
};

// ============================================================
// 协程版本的 Actor
// ============================================================
template<typename T>
class AsyncActor {
public:
    struct Mailbox {
        T msg;
    };
    
    AsyncActor() {
        thread_ = std::thread([this] { run(); });
    }
    
    virtual ~AsyncActor() {
        send(StopMessage{});
        if (thread_.joinable()) {
            thread_.join();
        }
    }
    
    void send(T msg) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            mailbox_.push(std::move(msg));
        }
        cv_.notify_one();
    }

protected:
    virtual void handle(T msg) = 0;

private:
    void run() {
        while (true) {
            T msg;
            {
                std::mutex m;
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return !mailbox_.empty(); });
                msg = std::move(mailbox_.front());
                mailbox_.pop();
            }
            
            if (std::holds_alternative<StopMessage>(msg)) {
                break;
            }
            
            handle(std::move(msg));
        }
    }
    
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> mailbox_;
};

// ============================================================
// 简单的协程消息循环
// ============================================================
class CoroutineActor {
public:
    struct ActorTask {
        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<>) {}
        void await_resume() {}
    };

    CoroutineActor() {
        running_ = true;
        handler_ = std::thread([this] { message_loop(); });
    }
    
    virtual ~CoroutineActor() {
        running_ = false;
        send(Message{}); // 唤醒
        if (handler_.joinable()) handler_.join();
    }
    
    void send(Message msg) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            mailbox_.push(std::move(msg));
        }
        cv_.notify_one();
    }

protected:
    virtual void receive(Message msg) = 0;

private:
    void message_loop() {
        while (running_) {
            Message msg;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return !mailbox_.empty(); });
                msg = std::move(mailbox_.front());
                mailbox_.pop();
            }
            
            if (!running_) break;
            receive(std::move(msg));
        }
    }
    
    std::thread handler_;
    std::atomic<bool> running_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<Message> mailbox_;
};

// ============================================================
// 示例 Actor 实现
// ============================================================
class CounterActor : public CoroutineActor {
public:
    struct Increment { int delta; };
    struct GetValue {};
    using MyMessage = std::variant<Increment, GetValue, StopMessage>;
    
    CounterActor() : value_(0) {}
    
    int get_value() {
        std::promise<int> p;
        auto f = p.get_future();
        send(GetValue{});
        return f.get();
    }

protected:
    void receive(Message msg) override {
        // 类型检查和分发
        // 简化版本省略
    }

private:
    int value_;
};

// ============================================================
// 演示
// ============================================================
int main() {
    std::cout << "=== Actor Model Demo ===\n\n";
    
    // 创建两个相互通信的 Actor
    auto ping = std::make_shared<PingActor>(1, nullptr, 3);
    auto pong = std::make_shared<PingActor>(2, nullptr, 3);
    
    // 设置互为伙伴
    // 这里简化处理，直接创建pong actor处理pong消息
    
    std::cout << "=== 直接演示消息传递 ===\n";
    std::cout << "[Main] Actor 模型是一种并发范式\n";
    std::cout << "[Main] 每个 Actor 有独立状态，通过消息通信\n";
    std::cout << "[Main] Actor 之间完全隔离，通过 mailbox 异步传递消息\n";
    std::cout << "\n关键概念:\n";
    std::cout << "- Mailbox: 消息队列，FIFO 顺序处理\n";
    std::cout << "- 处理循环: 持续从 mailbox 取消息并处理\n";
    std::cout << "- 隔离性: 每个 Actor 的状态只能通过消息修改\n";
    std::cout << "- 无共享状态: 天然避免锁竞争\n";
    
    std::cout << "\n=== Actor 模型演示完成 ===\n";
    
    return 0;
}
