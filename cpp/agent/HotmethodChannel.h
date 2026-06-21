#ifndef MINI_DROP_HOTMETHODCHANNEL_H
#define MINI_DROP_HOTMETHODCHANNEL_H

// ============================================================
// HotmethodChannel — 任务执行线程
//
// 从内部队列阻塞取任务 → 创建 IProfiler → 执行采集 → 上报结果。
// 心跳线程通过 PushTask() 向本队列投递任务。
// ============================================================

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include <grpcpp/grpcpp.h>
#include "proto/hotmethod.grpc.pb.h"
#include "common/IProfiler.h"

class HotmethodChannel {
public:
    HotmethodChannel(std::shared_ptr<grpc::Channel> channel,
                     std::atomic<bool>*      running);

    // 心跳线程调用：把任务投递到内部队列
    void PushTask(const drop::hotmethod::TaskDesc& task);

    // 工作线程主循环（在新的 std::thread 里调用）
    void Run();

private:
    std::queue<drop::hotmethod::TaskDesc> queue_;
    std::mutex               mutex_;
    std::condition_variable  cv_;

    std::unique_ptr<drop::hotmethod::Hotmethod::Stub> stub_;
    std::atomic<bool>* running_;
};

#endif // MINI_DROP_HOTMETHODCHANNEL_H