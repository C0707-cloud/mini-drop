#include "ControlService.h"
#include "HealthCheckService.h"

#include <iostream>
#include <random>
#include <sstream>

// 生成简短任务 ID
static std::string GenTaskId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    std::ostringstream oss;
    oss << "task-" << dis(gen);
    return oss.str();
}

ControlService::ControlService(
        TaskQueue<drop::hotmethod::TaskDesc>* taskQueue,
        HealthCheckService*                    healthService)
    : taskQueue_(taskQueue)
    , healthService_(healthService) {}

grpc::Status ControlService::CreateTask(
        grpc::ServerContext* ctx,
        const drop::control::CreateTaskRequest* req,
        drop::control::CreateTaskResponse* resp) {

    std::string targetIp = req->target_ip();

    // 检查 Agent 是否在线
    auto agents = healthService_->GetAgents();
    bool online = false;
    for (const auto& a : agents) {
        if (a.ip_addr == targetIp && a.is_online) {
            online = true;
            break;
        }
    }

    if (!online) {
        resp->set_error("Agent " + targetIp + " 离线或不存在");
        return grpc::Status::OK;
    }

    // 构造 TaskDesc
    drop::hotmethod::TaskDesc taskDesc = req->task_desc();
    std::string tid = GenTaskId();
    taskDesc.set_task_id(tid);

    // 放入任务队列
    taskQueue_->PushBack(targetIp, taskDesc);

    resp->set_task_id(tid);
    std::cout << "[ControlService] 创建任务 " << tid
              << " → " << targetIp << std::endl;

    return grpc::Status::OK;
}

grpc::Status ControlService::ListAgents(
        grpc::ServerContext* ctx,
        const drop::control::ListAgentsRequest* req,
        drop::control::ListAgentsResponse* resp) {

    auto agents = healthService_->GetAgents();
    for (const auto& a : agents) {
        auto* info = resp->add_agents();
        info->set_uid(a.uid);
        info->set_hostname(a.hostname);
        info->set_ip_addr(a.ip_addr);
        info->set_agent_version(a.agent_version);
        info->set_is_online(a.is_online);
    }

    return grpc::Status::OK;
}

grpc::Status ControlService::StatAgent(
        grpc::ServerContext* ctx,
        const drop::control::StatAgentRequest* req,
        drop::control::StatAgentResponse* resp) {

    resp->set_is_online(false);
    auto agents = healthService_->GetAgents();
    for (const auto& a : agents) {
        if (a.ip_addr == req->target_ip()) {
            resp->set_is_online(a.is_online);
            break;
        }
    }

    return grpc::Status::OK;
}

grpc::Status ControlService::FetchData(
        grpc::ServerContext* ctx,
        const drop::control::FetchDataRequest* req,
        drop::control::FetchDataResponse* resp) {

    // 简化实现：暂不实现文件拉取
    resp->set_error("FetchData 暂未实现");
    return grpc::Status::OK;
}
