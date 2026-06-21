// ============================================================
// Mini-Drop Day 6 · ProcStats 单元测试（3 个用例）
//
// 覆盖：读自身进程、无效 PID、Delta 计算
// ============================================================

#include "../common/ProcStats.h"
#include <iostream>
#include <unistd.h>
#include <cassert>

static int g_passed = 0;
static int g_failed = 0;

#define TEST(name)  std::cout << "[TEST] " << name << " ... " << std::flush
#define PASS()      do { std::cout << "通过" << std::endl; g_passed++; } while(0)
#define FAIL(msg)   do { std::cout << "失败: " << msg << std::endl; g_failed++; } while(0)

int main() {
    // 测试1：读自身进程 /proc/stat 数据
    TEST("读自身进程");
    {
        int pid = getpid();
        ProcSnapshot snap = ProcStats::Snapshot(pid);
        // 自身进程的 RSS 肯定 > 0
        if (snap.rss > 0) PASS(); else FAIL("自身 RSS 应该 > 0");
    }

    // 测试2：无效 PID 返回空快照
    TEST("无效 PID 返回空数据");
    {
        ProcSnapshot snap = ProcStats::Snapshot(999999);
        // 不存在的 PID 应该返回全 0
        if (snap.utime == 0 && snap.stime == 0 && snap.rss == 0)
            PASS();
        else
            FAIL("无效 PID 应该全 0");
    }

    // 测试3：ComputeDelta 计算 CPU% 和 RSS
    TEST("ComputeDelta 计算正确");
    {
        int pid = getpid();
        ProcSnapshot prev = ProcStats::Snapshot(pid);
        // 忙等一小段消耗 CPU
        volatile int x = 0;
        for (int i = 0; i < 1000000; ++i) x += i;
        ProcSnapshot curr = ProcStats::Snapshot(pid);

        ProcDelta delta = ProcStats::ComputeDelta(prev, curr);
        // CPU% 应该 ≥ 0
        if (delta.cpu_percent >= 0.0 && delta.rss_kb > 0)
            PASS();
        else
            FAIL("计算异常");
    }

    // 结果汇总
    std::cout << "\n=== 结果: " << g_passed << " 通过, "
              << g_failed << " 失败 ===" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
