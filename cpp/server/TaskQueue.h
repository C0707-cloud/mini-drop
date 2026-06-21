#ifndef MINI_DROP_TASKQUEUE_H
#define MINI_DROP_TASKQUEUE_H

// ============================================================
// Mini-Drop Day 2.2 · 线程安全任务队列
//
// 与 SafeQueue 的关键区别：
//   - 按 IP 分组（每个 Agent 独立队列）
//   - TryPop 非阻塞（心跳线程不能等）
//   - PushFront 支持紧急任务插队
// ============================================================

#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <string>

template <typename TaskT>
class TaskQueue {
public:
    TaskQueue()  = default;
    ~TaskQueue() = default;

    TaskQueue(const TaskQueue&)            = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    void PushBack(const std::string& ip, TaskT task) {
        std::lock_guard<std::mutex> lock(mutex_);
        queues_[ip].push_back(std::move(task));
    }

    void PushFront(const std::string& ip, TaskT task) {
        std::lock_guard<std::mutex> lock(mutex_);
        queues_[ip].push_front(std::move(task));
    }

    std::optional<TaskT> TryPop(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = queues_.find(ip);
        if (it == queues_.end() || it->second.empty()) {
            return std::nullopt;
        }
        TaskT task = std::move(it->second.front());
        it->second.pop_front();
        return task;
    }

    size_t PendingCount(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = queues_.find(ip);
        if (it == queues_.end()) return 0;
        return it->second.size();
    }

    size_t TotalPending() {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t total = 0;
        for (const auto& [ip, q] : queues_) total += q.size();
        return total;
    }

private:
    std::map<std::string, std::deque<TaskT>> queues_;
    std::mutex mutex_;
};

#endif // MINI_DROP_TASKQUEUE_H
