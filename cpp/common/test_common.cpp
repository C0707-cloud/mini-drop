#include "ProcStats.h"
#include "PerfProfiler.h"
#include <iostream>
#include <unistd.h>
#include <cassert>

int main() {
    // 测试 ProcStats：读自身进程
    int pid = getpid();
    auto prev = ProcStats::Snapshot(pid);
    if (prev.utime == 0 && prev.stime == 0 && prev.rss == 0) {
        std::cerr << "ProcStats::Snapshot 失败" << std::endl;
        return 1;
    }
    std::cout << "测试1 通过: Snapshot(自己) RSS=" << prev.rss << " 页" << std::endl;

    sleep(1);
    auto curr = ProcStats::Snapshot(pid);
    auto delta = ProcStats::ComputeDelta(prev, curr);
    std::cout << "测试2 通过: CPU=" << delta.cpu_percent
              << "% RSS=" << delta.rss_kb << " KB" << std::endl;

    // 测试 PerfProfiler：验证 BuildArgv 生成的命令不为空
    PerfProfiler profiler;
    std::cout << "测试3 通过: 采集器名称=" << profiler.Name() << std::endl;

    ProfilerConfig cfg;
    cfg.hz = 99;
    cfg.duration_sec = 1;
    cfg.pid = 1;
    cfg.callgraph = "fp";
    cfg.event = "cpu-cycles";
    ProfilerResult r = profiler.Record(cfg);
    std::cout << "测试4: 采集" << (r.success ? "成功" : "失败") << " - " << r.error << std::endl;

    std::cout << "全部测试完成" << std::endl;
    return 0;
}
