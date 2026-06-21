// ============================================================
// drop_server — Mini-Drop Server 入口
//
// 单进程注册 3 个 gRPC Service：
//   HealthCheckService — 心跳 + 任务捎带
//   ControlService     — Web API 入口
//   HotmethodService   — 结果收集
//
// 额外启动离线检测线程（每 10s 扫描，30s 无心跳判离线）
// ============================================================

#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "HealthCheckService.h"
#include "ControlService.h"
#include "HotmethodService.h"

std::atomic<bool> g_serverRunning{true};

void SignalHandler(int) {
    g_serverRunning = false;
}

int main() {
    const std::string listenAddr = "0.0.0.0:50051";

    // 1. 信号处理
    signal(SIGINT,  SignalHandler);
    signal(SIGTERM, SignalHandler);

    // 2. 创建共享组件
    TaskQueue<drop::hotmethod::TaskDesc> taskQueue;
    HealthCheckService  healthService(&taskQueue);
    HotmethodService    hotmethodService;
    ControlService      controlService(&taskQueue, &healthService);

    // 3. 启动离线检测线程
    std::thread offlineDetector([&healthService]() {
        std::cout << "[离线检测] 线程启动" << std::endl;
        while (g_serverRunning) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            healthService.DetectOffline(30);
        }
        std::cout << "[离线检测] 线程退出" << std::endl;
    });

    // 4. 启动 gRPC Server
    grpc::ServerBuilder builder;
    builder.AddListeningPort(listenAddr, grpc::InsecureServerCredentials());
    builder.RegisterService(&healthService);
    builder.RegisterService(&controlService);
    builder.RegisterService(&hotmethodService);

    auto server = builder.BuildAndStart();
    std::cout << "Server 启动: " << listenAddr << std::endl;

    // 5. 等待退出信号
    server->Wait();

    g_serverRunning = false;
    offlineDetector.join();

    std::cout << "Server 已退出" << std::endl;
    return 0;
}
