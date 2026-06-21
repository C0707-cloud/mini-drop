#ifndef MINI_DROP_HOTMETHODSERVICE_H
#define MINI_DROP_HOTMETHODSERVICE_H

// ============================================================
// HotmethodService — 任务结果收集（Server 端实现）
//
// Agent 采集完成后调 NotifyResult() 上报结果。
// ============================================================

#include <map>
#include <mutex>
#include <string>

#include "proto/hotmethod.grpc.pb.h"

struct TaskRecord {
    std::string task_id;
    std::string target_ip;
    std::string status;         // PENDING / RUNNING / UPLOADING / DONE / FAILED
    std::string status_reason;
    std::string output_file;    // 结果文件路径
};

class HotmethodService : public drop::hotmethod::Hotmethod::Service {
public:
    // Agent 上报采集结果
    grpc::Status NotifyResult(grpc::ServerContext* ctx,
                              const drop::hotmethod::TaskResult* req,
                              google::protobuf::Empty* resp) override;

    // 查询任务结果
    std::optional<TaskRecord> GetTask(const std::string& taskId);

    // 获取所有任务记录
    std::vector<TaskRecord> GetTasks();

private:
    std::map<std::string, TaskRecord> tasks_;
    std::mutex tasksMutex_;
};

#endif // MINI_DROP_HOTMETHODSERVICE_H
