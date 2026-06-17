#ifndef MINI_DROP_PROCSTATS_H
#define MINI_DROP_PROCSTATS_H

// ============================================================
// ProcStats — Linux /proc 文件系统解析器
//
// 读 /proc/[pid]/stat 获取进程 CPU 和内存数据。
// 两次快照做差，计算 CPU% 和 RSS。
// Agent 心跳线程用此类采集自身资源占用。
// ============================================================

#include <cstdint>
#include <string>

struct ProcSnapshot {
    uint64_t utime;    // 用户态 CPU 时间 (ticks)
    uint64_t stime;    // 内核态 CPU 时间 (ticks)
    uint64_t rss;      // 物理内存 (页)
    uint64_t total;    // 系统总 CPU 时间 (ticks)
};

struct ProcDelta {
    double   cpu_percent;   // CPU 使用率 (%)
    uint64_t rss_kb;        // 物理内存 (KB)
};

class ProcStats {
public:
    // 读取某个 PID 的 proc 快照
    static ProcSnapshot Snapshot(int pid);

    // 两次快照做差
    static ProcDelta ComputeDelta(const ProcSnapshot& prev,
                                  const ProcSnapshot& curr);
};

#endif // MINI_DROP_PROCSTATS_H
