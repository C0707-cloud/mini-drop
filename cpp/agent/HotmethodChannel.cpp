#include "HotmethodChannel.h"
#include "common/ProcStats.h"
#include "common/PerfProfiler.h"

#include <iostream>
#include <thread>

HotmethodChannel::HotmethodChannel(
        std::shared_ptr<grpc::Channel> channel,
        std::atomic<bool>*      running)
    : stub_(drop::hotmethod::Hotmethod::NewStub(channel))
    , running_(running) {}

void HotmethodChannel::PushTask(const drop::hotmethod::TaskDesc& task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(task);
    }
    cv_.notify_one();
}

void HotmethodChannel::Run() {
    std::cout << "[HotmethodChannel] 工作线程启动" << std::endl;

    while (*running_) {
        // 1. 阻塞等待任务
        drop::hotmethod::TaskDesc task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return !queue_.empty() || !*running_;
            });
            if (!*running_) break;
            if (queue_.empty()) continue;

            task = std::move(queue_.front());
            queue_.pop();
        }

        std::cout << "[HotmethodChannel] 开始执行任务: "
                  << task.task_id() << std::endl;

        // 2. 根据 TaskDesc 参数创建 ProfilerConfig
        ProfilerConfig cfg;
        cfg.hz           = task.sample_argv().hz();
        cfg.duration_sec = task.sample_argv().duration_sec();
        cfg.pid          = task.sample_argv().pid();
        cfg.callgraph    = task.sample_argv().callgraph();
        cfg.event        = task.sample_argv().event();

        // 3. 执行采集
        PerfProfiler profiler;
        ProfilerResult result = profiler.Record(cfg);

        // 4. 构建上报结果
        drop::hotmethod::TaskResult taskResult;
        taskResult.set_task_id(task.task_id());

        if (result.success) {
            taskResult.set_error_message("");
            taskResult.set_cos_key(result.output_file);
        } else {
            taskResult.set_error_message(result.error);
        }

        // 5. 采集期间自身资源快照
        int myPid = getpid();
        ProcSnapshot prev = ProcStats::Snapshot(myPid);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ProcSnapshot curr = ProcStats::Snapshot(myPid);
        ProcDelta delta = ProcStats::ComputeDelta(prev, curr);

        auto* selfStats = taskResult.add_self_pstats();
        selfStats->set_cpu_percent(delta.cpu_percent);
        selfStats->set_rss_kb(delta.rss_kb);
        selfStats->set_pid(myPid);

        // 6. 上报结果给 Server
        grpc::ClientContext ctx;
        ctx.set_deadline(std::chrono::system_clock::now()
                         + std::chrono::seconds(10));
        google::protobuf::Empty empty;
        grpc::Status status = stub_->NotifyResult(&ctx, taskResult, &empty);

        if (!status.ok()) {
            std::cerr << "[HotmethodChannel] 上报失败: "
                      << status.error_message() << std::endl;
        } else {
            std::cout << "[HotmethodChannel] 任务 " << task.task_id()
                      << " 完成" << std::endl;
        }
    }

    std::cout << "[HotmethodChannel] 工作线程退出" << std::endl;
}