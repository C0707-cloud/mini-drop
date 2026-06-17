#ifndef MINI_DROP_IPROFILER_H
#define MINI_DROP_IPROFILER_H

// ============================================================
// IProfiler — 采集器策略接口（开闭原则）
//
// 新增采集器只需实现此接口，不修改任何现有代码。
// PerfProfiler / EbpfProfiler / PprofProfiler 都继承它。
// ============================================================

#include <string>

struct ProfilerConfig {
    int    hz;            // 采样频率 (Hz)
    int    duration_sec;  // 采集时长 (秒)
    int    pid;           // 目标进程 PID
    std::string callgraph; // callgraph 模式: fp / dwarf / lbr
    std::string event;     // perf event: cpu-cycles / cache-misses
};

struct ProfilerResult {
    std::string output_file;  // 采集产出的文件路径
    std::string error;        // 错误信息（空 = 成功）
    bool        success;      // 是否成功
};

class IProfiler {
public:
    virtual ~IProfiler() = default;

    // 返回采集器名称，如 "perf"、"ebpf"、"pprof"
    virtual const char* Name() const = 0;

    // 执行采集，返回结果
    virtual ProfilerResult Record(const ProfilerConfig& cfg) = 0;
};

#endif // MINI_DROP_IPROFILER_H
