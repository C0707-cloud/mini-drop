#ifndef MINI_DROP_CONTROLSERVICE_H
#define MINI_DROP_CONTROLSERVICE_H

// ============================================================
// ControlService — Web API 入口（Server 端实现）
//
// apiserver 调此 Service 创建任务、查询 Agent、拉取数据。
// ============================================================

#include "proto/control.grpc.pb.h"
#include "TaskQueue.h"
#include "HealthCheckService.h"

class ControlService : public drop::control::Control::Service {
public:
    ControlService(TaskQueue<drop::hotmethod::TaskDesc>*  taskQueue,
                   HealthCheckService*                     healthService);

    // 创建采集任务
    grpc::Status CreateTask(grpc::ServerContext* ctx,
                            const drop::control::CreateTaskRequest* req,
                            drop::control::CreateTaskResponse* resp) override;

    // 列出所有 Agent
    grpc::Status ListAgents(grpc::ServerContext* ctx,
                            const drop::control::ListAgentsRequest* req,
                            drop::control::ListAgentsResponse* resp) override;

    // 查询某个 Agent 的资源占用
    grpc::Status StatAgent(grpc::ServerContext* ctx,
                           const drop::control::StatAgentRequest* req,
                           drop::control::StatAgentResponse* resp) override;

    // 拉取任务数据
    grpc::Status FetchData(grpc::ServerContext* ctx,
                           const drop::control::FetchDataRequest* req,
                           drop::control::FetchDataResponse* resp) override;

private:
    TaskQueue<drop::hotmethod::TaskDesc>* taskQueue_;
    HealthCheckService*                    healthService_;
};

#endif // MINI_DROP_CONTROLSERVICE_H
