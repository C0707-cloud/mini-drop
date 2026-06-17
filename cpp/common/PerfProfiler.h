#ifndef MINI_DROP_PERFPROFILER_H
#define MINI_DROP_PERFPROFILER_H

// ============================================================
// PerfProfiler — Linux perf 采集器
//
// 把 ProfilerConfig 翻译成 perf record 命令行，
// fork 子进程执行，超时 killpg，waitpid 回收。
// ============================================================

#include "IProfiler.h"
#include <vector>
#include <string>

class PerfProfiler : public IProfiler {
public:
    const char* Name() const override { return "perf"; }

    ProfilerResult Record(const ProfilerConfig& cfg) override;

private:
    // 把 ProfilerConfig 翻译成 perf record 的命令行参数数组
    // 例如: perf record -F 99 -p 1234 --call-graph fp -e cpu-cycles -g -- sleep 10
    std::vector<std::string> BuildArgv(const ProfilerConfig& cfg);

    // fork + setpgid + execvp + 超时杀 + waitpid
    // 返回子进程退出码，超时被杀死返回 -1
    int ExecuteWithTimeout(const std::vector<std::string>& argv,
                           int timeoutSec);
};

#endif // MINI_DROP_PERFPROFILER_H
