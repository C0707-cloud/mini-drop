#include "TaskQueue.h"
#include <iostream>
#include <thread>
#include <cassert>

int main() {
    TaskQueue<int> q;

    auto r = q.TryPop("10.0.0.1");
    assert(!r.has_value());
    std::cout << "测试1 通过: 空队列返回 nullopt" << std::endl;

    q.PushBack("10.0.0.1", 42);
    r = q.TryPop("10.0.0.1");
    assert(r.has_value() && r.value() == 42);
    std::cout << "测试2 通过: PushBack + TryPop" << std::endl;

    q.PushBack("10.0.0.1", 1);
    q.PushFront("10.0.0.1", 99);
    r = q.TryPop("10.0.0.1");
    assert(r.value() == 99);
    std::cout << "测试3 通过: PushFront 插队成功" << std::endl;

    q.PushBack("10.0.0.1", 100);
    q.PushBack("10.0.0.2", 200);
    assert(q.PendingCount("10.0.0.1") == 1);
    assert(q.PendingCount("10.0.0.2") == 1);
    std::cout << "测试4 通过: IP 独立队列" << std::endl;

    assert(q.TotalPending() == 2);
    std::cout << "测试5 通过: TotalPending = " << q.TotalPending() << std::endl;

    std::cout << "全部测试通过" << std::endl;
    return 0;
}
