#include "HotmethodService.h"

#include <iostream>

grpc::Status HotmethodService::NotifyResult(
        grpc::ServerContext* ctx,
        const drop::hotmethod::TaskResult* req,
        google::protobuf::Empty* resp) {

    std::string tid = req->task_id();

    std::lock_guard<std::mutex> lock(tasksMutex_);

    TaskRecord& rec = tasks_[tid];
    rec.task_id = tid;

    if (req->error_message().empty()) {
        rec.status = "DONE";
        rec.status_reason = "";
        rec.output_file = req->cos_key();
        std::cout << "[HotmethodService] 任务 " << tid
                  << " 完成 → " << rec.output_file << std::endl;
    } else {
        rec.status = "FAILED";
        rec.status_reason = req->error_message();
        std::cerr << "[HotmethodService] 任务 " << tid
                  << " 失败: " << rec.status_reason << std::endl;
    }

    return grpc::Status::OK;
}

std::optional<TaskRecord> HotmethodService::GetTask(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    auto it = tasks_.find(taskId);
    if (it != tasks_.end()) return it->second;
    return std::nullopt;
}

std::vector<TaskRecord> HotmethodService::GetTasks() {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    std::vector<TaskRecord> list;
    for (const auto& [id, rec] : tasks_) {
        list.push_back(rec);
    }
    return list;
}
