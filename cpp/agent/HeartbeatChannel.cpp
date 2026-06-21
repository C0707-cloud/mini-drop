#include "HeartbeatChannel.h"
#include "common/ProcStats.h"

#include <iostream>
#include <thread>
#include <chrono>

HeartbeatChannel::HeartbeatChannel(
        const std::string& hostname,
        const std::string& ip,
        const std::string& agentId,
        const std::string& version,
        std::shared_ptr<grpc::Channel> channel,
        TaskQueue<drop::hotmethod::TaskDesc>* queue,
        std::atomic<bool>* running)
    : hostname_(hostname)
    , ip_(ip)
    , agentId_(agentId)
    , version_(version)
    , stub_(drop::healthcheck::HealthCheck::NewStub(channel))
    , queue_(queue)
    , running_(running) {}

void HeartbeatChannel::Run() {
    std::cout << "[HeartbeatChannel] 心跳线程启动" << std::endl;

    while (*running_) {
        // 1. 读取自身资源占用
        int myPid = getpid();
        ProcSnapshot prev = ProcStats::Snapshot(myPid);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ProcSnapshot curr = ProcStats::Snapshot(myPid);
        ProcDelta delta = ProcStats::ComputeDelta(prev, curr);

        // 2. 构建心跳请求
        drop::healthcheck::HealthCheckRequest req;
        req.set_hostname(hostname_);
        req.set_ip_addr(ip_);
        req.set_uid(agentId_);
        req.set_agent_version(version_);

        auto* selfStats = req.mutable_self_pstats();
        selfStats->set_cpu_percent(delta.cpu_percent);
        selfStats->set_rss_kb(delta.rss_kb);
        selfStats->set_pid(myPid);

        // 3. 发送心跳
        drop::healthcheck::HealthCheckResponse resp;
        grpc::ClientContext ctx;
        ctx.set_deadline(std::chrono::system_clock::now()
                         + std::chrono::seconds(5));

        grpc::Status status = stub_->Do(&ctx, req, &resp);

        if (!status.ok()) {
            std::cerr << "[HeartbeatChannel] 心跳失败: "
                      << status.error_message() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        std::cout << "[HeartbeatChannel] 心跳 OK" << std::endl;

        // 4. 收到捎带任务 → 放入 TaskQueue
        if (resp.has_pending_task()) {
            std::cout << "[HeartbeatChannel] 收到任务: "
                      << resp.task_desc().task_id() << std::endl;
            queue_->PushBack(ip_, resp.task_desc());
        }

        // 5. 等待下一轮心跳
        for (int i = 0; i < 5 && *running_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    std::cout << "[HeartbeatChannel] 心跳线程退出" << std::endl;
}
