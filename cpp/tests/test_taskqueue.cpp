// ============================================================
// Mini-Drop Day 6 · TaskQueue 单元测试（6 个用例）
//
// 覆盖：空队列、PushBack、TryPop、PushFront插队、IP隔离、PendingCount
// ============================================================

#include "../server/TaskQueue.h"
#include <iostream>
#include <thread>
#include <cassert>

static int g_passed = 0;
static int g_failed = 0;

#define TEST(name)  std::cout << "[TEST] " << name << " ... " << std::flush
#define PASS()      do { std::cout << "通过" << std::endl; g_passed++; } while(0)
#define FAIL(msg)   do { std::cout << "失败: " << msg << std::endl; g_failed++; } while(0)

int main() {
    // 测试1：空队列 TryPop 返回 nullopt
    TEST("空队列 TryPop 返回 nullopt");
    {
        TaskQueue<int> q;
        auto r = q.TryPop("10.0.0.1");
        if (!r.has_value()) PASS(); else FAIL("应该返回 nullopt");
    }

    // 测试2：PushBack 后 TryPop 拿到正确值
    TEST("PushBack + TryPop 基本流程");
    {
        TaskQueue<int> q;
        q.PushBack("10.0.0.1", 42);
        auto r = q.TryPop("10.0.0.1");
        if (r.has_value() && r.value() == 42) PASS(); else FAIL("应该拿到 42");
    }

    // 测试3：FIFO 顺序 —— 先 Push 的先 Pop
    TEST("FIFO 顺序");
    {
        TaskQueue<int> q;
        q.PushBack("10.0.0.1", 1);
        q.PushBack("10.0.0.1", 2);
        q.PushBack("10.0.0.1", 3);
        bool ok = true;
        auto r1 = q.TryPop("10.0.0.1");
        auto r2 = q.TryPop("10.0.0.1");
        auto r3 = q.TryPop("10.0.0.1");
        if (!r1.has_value() || r1.value() != 1) ok = false;
        if (!r2.has_value() || r2.value() != 2) ok = false;
        if (!r3.has_value() || r3.value() != 3) ok = false;
        if (ok) PASS(); else FAIL("顺序应该是 1-2-3");
    }

    // 测试4：PushFront 插队
    TEST("PushFront 插队到头部");
    {
        TaskQueue<int> q;
        q.PushBack("10.0.0.1", 1);
        q.PushFront("10.0.0.1", 99);
        auto r1 = q.TryPop("10.0.0.1");
        auto r2 = q.TryPop("10.0.0.1");
        if (r1.value() == 99 && r2.value() == 1) PASS(); else FAIL("99 应该在 1 之前");
    }

    // 测试5：不同 IP 独立队列
    TEST("不同 IP 队列隔离");
    {
        TaskQueue<int> q;
        q.PushBack("10.0.0.1", 100);
        q.PushBack("10.0.0.2", 200);
        auto r1 = q.TryPop("10.0.0.1");
        auto r2 = q.TryPop("10.0.0.2");
        bool ok = (r1.value() == 100) && (r2.value() == 200);
        if (ok) PASS(); else FAIL("IP 隔离失败");
    }

    // 测试6：PendingCount 统计正确
    TEST("PendingCount 统计");
    {
        TaskQueue<int> q;
        q.PushBack("10.0.0.1", 1);
        q.PushBack("10.0.0.1", 2);
        q.PushBack("10.0.0.2", 3);
        if (q.PendingCount("10.0.0.1") == 2 &&
            q.PendingCount("10.0.0.2") == 1 &&
            q.TotalPending() == 3) PASS(); else FAIL("统计不对");
    }

    // 结果汇总
    std::cout << "\n=== 结果: " << g_passed << " 通过, "
              << g_failed << " 失败 ===" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
