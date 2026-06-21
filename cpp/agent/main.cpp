// ============================================================
// drop_agent — Mini-Drop Agent 入口
//
// 启动两个线程：
//   HeartbeatChannel — 心跳线程（每 5s）
//   HotmethodChannel — 任务执行线程
//
// 支持命令行参数 -c <config.json>
// 支持 SIGINT / SIGTERM 优雅退出
// ============================================================

#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <getopt.h>

#include "Config.h"
#include "HeartbeatChannel.h"
#include "HotmethodChannel.h"
#include "cpp/server/TaskQueue.h"
#include "proto/hotmethod.pb.h"

std::atomic<bool> g_running{true};

void SignalHandler(int) {
    g_running = false;
}

int main(int argc, char* argv[]) {
    // 1. 解析命令行
    std::string configPath = "config.json";
    int opt;
    while ((opt = getopt_long(argc, argv, "c:h", nullptr, nullptr)) != -1) {
        switch (opt) {
            case 'c': configPath = optarg; break;
            case 'h':
            default:
                std::cout << "用法: " << argv[0] << " -c <config.json>" << std::endl;
                return 1;
        }
    }

    // 2. 加载配置
    AgentConfig cfg;
    try {
        cfg = Config::LoadFromFile(configPath);
        Config::ApplyEnvOverrides(cfg);
    } catch (const std::exception& e) {
        std::cerr << "加载配置失败: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Agent ID: " << cfg.agent_id << std::endl;
    std::cout << "Server:   " << cfg.server_ips[0] << std::endl;

    // 3. 信号处理
    signal(SIGINT,  SignalHandler);
    signal(SIGTERM, SignalHandler);

    // 4. 创建 gRPC Channel
    auto channel = grpc::CreateChannel(
        cfg.server_ips[0],
        grpc::InsecureChannelCredentials());

    // 5. 任务队列（心跳线程 → 工作线程）
    TaskQueue<drop::hotmethod::TaskDesc> taskQueue;

    // 6. 创建两个 Channel
    HeartbeatChannel heartbeat(
        cfg.hostname, cfg.ip_addr,
        cfg.agent_id, cfg.agent_version,
        channel, &taskQueue, &g_running);

    HotmethodChannel hotChannel(channel, &g_running);

    // 7. 启动两个线程
    std::thread heartbeatThread(&HeartbeatChannel::Run, &heartbeat);
    std::thread hotmethodThread(&HotmethodChannel::Run, &hotChannel);

    std::cout << "Agent 已启动，Ctrl+C 退出" << std::endl;

    heartbeatThread.join();
    hotmethodThread.join();

    std::cout << "Agent 已退出" << std::endl;
    return 0;
}
