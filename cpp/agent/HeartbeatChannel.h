#ifndef MINI_DROP_HEARTBEATCHANNEL_H
#define MINI_DROP_HEARTBEATCHANNEL_H

// ============================================================
// HeartbeatChannel — 心跳线程
//
// 每 5 秒向 Server 发送心跳，附带自身 PidStats。
// Server 在响应中捎带任务，本线程将其投入 TaskQueue。
// ============================================================

#include <atomic>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "proto/healthcheck.grpc.pb.h"
#include "cpp/server/TaskQueue.h"
#include "proto/hotmethod.pb.h"

class HeartbeatChannel {
public:
    HeartbeatChannel(const std::string& hostname,
                     const std::string& ip,
                     const std::string& agentId,
                     const std::string& version,
                     std::shared_ptr<grpc::Channel> channel,
                     TaskQueue<drop::hotmethod::TaskDesc>* queue,
                     std::atomic<bool>* running);

    void Run();

private:
    std::string hostname_;
    std::string ip_;
    std::string agentId_;
    std::string version_;

    std::unique_ptr<drop::healthcheck::HealthCheck::Stub> stub_;
    TaskQueue<drop::hotmethod::TaskDesc>* queue_;
    std::atomic<bool>* running_;
};

#endif // MINI_DROP_HEARTBEATCHANNEL_H
