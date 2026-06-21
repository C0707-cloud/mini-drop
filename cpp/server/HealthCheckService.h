#ifndef MINI_DROP_HEALTHCHECKSERVICE_H
#define MINI_DROP_HEALTHCHECKSERVICE_H

// ============================================================
// HealthCheckService — 心跳 gRPC Service（Server 端实现）
//
// Agent 每 5 秒调一次 Do()。
// Server 更新心跳时间 → 从 TaskQueue TryPop 任务 → 捎带返回。
// ============================================================

#include <map>
#include <mutex>
#include <string>
#include <chrono>

#include "proto/healthcheck.grpc.pb.h"
#include "TaskQueue.h"

struct AgentRecord {
    std::string hostname;
    std::string ip_addr;
    std::string uid;
    std::string agent_version;
    bool        is_online;
    std::chrono::steady_clock::time_point last_heartbeat;
};

class HealthCheckService : public drop::healthcheck::HealthCheck::Service {
public:
    HealthCheckService(TaskQueue<drop::hotmethod::TaskDesc>* taskQueue);

    grpc::Status Do(grpc::ServerContext* ctx,
                    const drop::healthcheck::HealthCheckRequest* req,
                    drop::healthcheck::HealthCheckResponse* resp) override;

    // 离线检测：扫描超时 Agent，返回离线 IP 列表
    std::vector<std::string> DetectOffline(int timeoutSec);

    // 获取 Agent 列表
    std::vector<AgentRecord> GetAgents();

private:
    std::map<std::string, AgentRecord> agents_;
    std::mutex agentsMutex_;
    TaskQueue<drop::hotmethod::TaskDesc>* taskQueue_;
};

#endif // MINI_DROP_HEALTHCHECKSERVICE_H
