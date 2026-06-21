#include "HealthCheckService.h"

#include <iostream>

HealthCheckService::HealthCheckService(
        TaskQueue<drop::hotmethod::TaskDesc>* taskQueue)
    : taskQueue_(taskQueue) {}

grpc::Status HealthCheckService::Do(
        grpc::ServerContext* ctx,
        const drop::healthcheck::HealthCheckRequest* req,
        drop::healthcheck::HealthCheckResponse* resp) {

    std::string ip = req->ip_addr();

    // 1. 更新 Agent 在线记录
    {
        std::lock_guard<std::mutex> lock(agentsMutex_);
        AgentRecord& rec = agents_[ip];
        rec.hostname       = req->hostname();
        rec.ip_addr        = ip;
        rec.uid            = req->uid();
        rec.agent_version  = req->agent_version();
        rec.is_online      = true;
        rec.last_heartbeat = std::chrono::steady_clock::now();
    }

    // 2. 从任务队列取一个待执行任务（非阻塞）
    auto task = taskQueue_->TryPop(ip);
    if (task.has_value()) {
        resp->set_has_pending_task(true);
        *resp->mutable_task_desc() = std::move(task.value());
        std::cout << "[HealthCheck] 捎带下发任务 " << resp->task_desc().task_id()
                  << " → " << ip << std::endl;
    } else {
        resp->set_has_pending_task(false);
    }

    resp->set_status(drop::healthcheck::HealthCheckResponse::SERVING);
    return grpc::Status::OK;
}

std::vector<std::string> HealthCheckService::DetectOffline(int timeoutSec) {
    std::vector<std::string> offline;
    auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(agentsMutex_);
    for (auto& [ip, rec] : agents_) {
        if (!rec.is_online) continue;
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                           now - rec.last_heartbeat).count();
        if (elapsed > timeoutSec) {
            rec.is_online = false;
            offline.push_back(ip);
            std::cout << "[离线检测] " << ip << " 已离线（" << elapsed << "秒无心跳）" << std::endl;
        }
    }
    return offline;
}

std::vector<AgentRecord> HealthCheckService::GetAgents() {
    std::lock_guard<std::mutex> lock(agentsMutex_);
    std::vector<AgentRecord> list;
    for (const auto& [ip, rec] : agents_) {
        list.push_back(rec);
    }
    return list;
}
