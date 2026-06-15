# Mini-Drop 学习路线：从 C++ 基础到分布式系统

> **使用方式**：每天完成对应章节的"概念学习"→ 做"热身练习"→ 写"项目代码"。遇到不懂的回来问我。

---

## 第 0 天：搞清楚你在做什么（30 分钟）

在写任何代码之前，把这个流程**画在纸上**：

```
你在浏览器点「开始采集」→ Flask 收到请求 → 通过 gRPC 告诉 drop_server
→ drop_server 把任务放进队列 → Agent 心跳时发现"有我的任务"
→ Agent fork 子进程跑 perf record → 跑完了告诉 Server
→ Analyzer 把 perf.data 转成火焰图 SVG → 浏览器展示
```

**检查你是否理解了**：闭卷画出上面的流程图，标出每个箭头的通信方式（HTTP? gRPC?）。

---

## 第 1 天：Hello gRPC —— 让两个进程对话

### 1.1 你要学什么

**核心概念：RPC（Remote Procedure Call，远程过程调用）**

你以前写的函数调用是「同一进程内」的：
```cpp
int result = add(1, 2);  // 直接调用
```

gRPC 让你做到「跨进程、跨机器」的函数调用：
```cpp
// 你的代码在机器A上
int result = stub->Add(1, 2);  // 实际在机器B上执行！
```

**为什么这个项目用 gRPC 而不是 HTTP？**
- HTTP：你需要手写 URL、method、JSON 解析，类型不安全
- gRPC：你定义 .proto 文件，自动生成类型安全的 C++ 代码，编译器帮你检查参数类型

### 1.2 热身练习：gRPC Hello World

**目标**：写一个 Echo 服务，Client 发一句话，Server 原样返回。

#### Step 1: 安装环境（Windows 用户）

**你必须用 Linux 环境**，因为这个项目依赖 perf、fork、/proc 等 Linux 特性。

推荐方案（按简单程度排序）：
1. **WSL2**（推荐）：Windows 自带，`wsl --install Ubuntu-22.04`
2. **Multipass**：`multipass launch 22.04`
3. **VirtualBox + Ubuntu 22.04 ISO**

进入 Linux 后执行：
```bash
# 安装编译工具链
sudo apt update
sudo apt install -y build-essential cmake git

# 安装 gRPC + Protobuf（这是最难的一步，耐心）
sudo apt install -y libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc

# 安装辅助库
sudo apt install -y libspdlog-dev nlohmann-json3-dev libgtest-dev

# 验证安装
which protoc          # 应该输出 /usr/bin/protoc
grpc_cpp_plugin --help  # 应该有输出
```

#### Step 2: 写你的第一个 .proto 文件

创建一个 `echo.proto`：

```proto
syntax = "proto3";

package tutorial;

// 定义"服务" = 一组可远程调用的函数
service Echo {
  // 定义一个 RPC 方法：输入 EchoRequest，返回 EchoResponse
  rpc Say(EchoRequest) returns (EchoResponse);
}

// 请求消息
message EchoRequest {
  string message = 1;
}

// 响应消息
message EchoResponse {
  string reply = 1;
}
```

**关键理解**：
- `service` = 接口定义，相当于 C++ 的抽象类
- `message` = 结构体，`= 1` 是字段编号（不是默认值！），用于二进制编码
- gRPC 会自动生成 `Echo::Service` 类（你继承它写实现）和 `Echo::Stub` 类（客户端用它调远程方法）

#### Step 3: 手写编译命令（理解 CMake 背后做了什么）

```bash
# 生成 C++ 代码（你先手动跑一遍，完全理解了再写 CMake）
protoc -I. --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) echo.proto

# 你会看到生成了 4 个文件：
# echo.pb.h / echo.pb.cc       → 消息的序列化代码
# echo.grpc.pb.h / echo.grpc.pb.cc → 服务的 stub/skeleton 代码
```

**打开 `echo.grpc.pb.h`**，找 `class Echo::Service` 和 `class Echo::Stub`，读一下生成的代码是什么样的。

#### Step 4: 写 Server 端（约 30 行）

```cpp
#include <grpcpp/grpcpp.h>
#include "echo.grpc.pb.h"

// 继承自动生成的抽象类，实现 Say 方法
class EchoImpl : public tutorial::Echo::Service {
  grpc::Status Say(grpc::ServerContext* ctx,
                   const tutorial::EchoRequest* req,
                   tutorial::EchoResponse* resp) override {
    resp->set_reply("Echo: " + req->message());  // 原样返回
    return grpc::Status::OK;
  }
};

int main() {
  EchoImpl service;
  grpc::ServerBuilder builder;
  builder.AddListeningPort("0.0.0.0:50051",
                           grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  server->Wait();
}
```

#### Step 5: 写 Client 端（约 20 行）

```cpp
#include <grpcpp/grpcpp.h>
#include "echo.grpc.pb.h"

int main() {
  auto channel = grpc::CreateChannel("localhost:50051",
                                     grpc::InsecureChannelCredentials());
  auto stub = tutorial::Echo::NewStub(channel);  // 这就是"远程对象"

  tutorial::EchoRequest req;
  req.set_message("Hello gRPC!");

  tutorial::EchoResponse resp;
  grpc::ClientContext ctx;
  auto status = stub->Say(&ctx, req, &resp);  // 远程调用！

  if (status.ok())
    std::cout << "Server 回复: " << resp.reply() << std::endl;
}
```

#### Step 6: 编译 + 运行

```bash
# 编译（先不用 CMake，手写 g++ 理解链接过程）
g++ -std=c++17 server.cpp echo.pb.cc echo.grpc.pb.cc \
    -lgrpc++ -lprotobuf -o server
g++ -std=c++17 client.cpp echo.pb.cc echo.grpc.pb.cc \
    -lgrpc++ -lprotobuf -o client

# 终端1
./server
# 终端2
./client
# 输出: Server 回复: Echo: Hello gRPC!
```

**✅ Day 1 验收标准**：你的 echo server/client 跑通，理解 `.proto → .pb.cc → server.cpp` 这条链。

### 1.3 Day 1 项目任务

**不要**直接写 Mini-Drop 的代码。先读懂我留给你的 5 个 `.proto` 文件：
- `common.proto`：通用类型，其他 proto 都 import 它
- `healthcheck.proto`：**关键**——注意 `HealthCheckResponse` 里嵌了 `hotmethod.TaskDesc`，这就是"心跳捎带任务"的设计
- `hotmethod.proto`：TaskDesc 的每一个字段都要能解释是干什么的
- `control.proto`：apiserver 调 drop_server 的入口
- `init.proto`：Agent 启动时的注册流程

**回答这三个问题**（写在你的设计文档里）：
1. 为什么心跳响应里要捎带 TaskDesc，而不是 Agent 单独调一个"取任务"的 RPC？（提示：少一次网络往返）
2. `RecordArgv` 里的每个字段分别对应 `perf record` 命令的哪个参数？
3. `NotifyResult` 为什么同时有 `File file` 和 `string cos_key` 两个字段？

---

## 第 2 天：多线程 —— 心跳线程和任务线程为什么要分开

### 2.1 你要学什么

**核心问题**：如果采集任务需要跑 10 秒，而心跳每 5 秒要发一次，怎么办？

```
❌ 错误做法（单线程）:
   while(true) {
     发心跳();
     if (有任务) 执行采集();  // 阻塞 10 秒！心跳断了！
   }

✅ 正确做法（双线程）:
   线程1: while(true) { 发心跳(); sleep(5); }    // 永远不被阻塞
   线程2: while(true) { 从队列取任务(); 执行(); }  // 独立运行
```

**你需要学会的 C++ 多线程基础**：
- `std::thread`：创建一个线程
- `std::mutex` + `std::lock_guard`：保护共享数据
- `std::condition_variable`：线程间通知（生产者-消费者模式）
- `std::atomic<bool>`：无锁的布尔标志（用于"是否该退出"）

### 2.2 热身练习：生产者-消费者队列

这是 Mini-Drop 任务队列的简化版。**先独立写完这个，理解了再写项目代码**。

```cpp
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

// TODO: 你来实现这个线程安全队列
template<typename T>
class SafeQueue {
private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;

public:
    // TODO 1: 向队列添加元素（生产者调用）
    void push(T item) {
        // 1. 加锁
        // 2. queue_.push(item)
        // 3. 通知一个等待的消费者
    }

    // TODO 2: 从队列取出元素（消费者调用），队列空时阻塞等待
    T pop() {
        // 1. 加锁
        // 2. cv_.wait(lock, [&]{ return !queue_.empty(); });
        // 3. T item = queue_.front(); queue_.pop();
        // 4. return item;
    }
};

int main() {
    SafeQueue<int> q;

    // 生产者线程：每秒往里丢一个数字
    std::thread producer([&q]() {
        for (int i = 0; i < 10; i++) {
            q.push(i);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    // 消费者线程：有就取，没有就等着
    std::thread consumer([&q]() {
        for (int i = 0; i < 10; i++) {
            int item = q.pop();
            std::cout << "消费: " << item << std::endl;
        }
    });

    producer.join();
    consumer.join();
    return 0;
}
```

**关键理解**：
- `cv_.wait(lock, condition)` 做了什么？——先解锁、睡觉、被唤醒时重新加锁、检查条件
- 为什么 `wait` 需要传入 `lock`？——检查条件和等待之间必须是原子的
- `notify_one()` vs `notify_all()` 的区别？

### 2.3 Day 2 项目任务

设计并写出 `TaskQueue.h`（放在 `cpp/server/` 下）：

**需求**：
1. 不是单队列，而是 `map<target_ip, deque<TaskDesc>>`——每个 Agent 独立队列
2. `PushBack(ip, task)` / `PushFront(ip, task)`——紧急任务插队用
3. `TryPop(ip)` —— 尝试取一个任务，没有返回 `std::nullopt`
4. 线程安全

**我会检查的**：
- `std::lock_guard` 用对了吗？
- 为什么用 `TryPop` 而不是 `Pop`？（提示：心跳线程不能阻塞等任务）

---

## 第 3 天：Linux 系统编程 —— fork, exec, /proc

### 3.1 你要学什么

这是 Agent 最核心的能力——**启动子进程执行采集命令，并管理其生命周期**。

**四个关键系统调用**：

| 调用 | 作用 | 类比 |
|------|------|------|
| `fork()` | 克隆当前进程 | 细胞分裂，产生一个一模一样的自己 |
| `execvp()` | 把当前进程替换成另一个程序 | 把自己变成别人 |
| `waitpid()` | 等待子进程结束 | 父母等孩子放学 |
| `killpg()` | 向整个进程组发信号 | 群发通知 |

**三个关键文件系统**：

| 路径 | 内容 | 你的代码要读什么 |
|------|------|-----------------|
| `/proc/[pid]/stat` | 进程 CPU 时间、内存、状态 | utime, stime, rss |
| `/proc/[pid]/io` | 进程磁盘读写字节数 | read_bytes, write_bytes |
| `/proc/stat` | 系统总 CPU 时间 | 第一行的所有数字之和 |

### 3.2 热身练习 1：fork + exec + 超时杀

写一个程序，fork 子进程跑 `sleep 100`，如果 3 秒内没结束就杀掉它：

```cpp
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>

int main() {
    pid_t pid = fork();  // TODO: 你要实现的

    if (pid == 0) {
        // TODO 子进程: 把自己变成 `sleep 100`
        // 提示: execlp("sleep", "sleep", "100", nullptr);
    } else {
        // TODO 父进程: 等 3 秒，还在跑就杀掉
        // 提示: sleep(3); 然后 waitpid(pid, &status, WNOHANG);
        //       如果还在跑，kill(pid, SIGTERM);
        //       再等 2 秒，还没死就 kill(pid, SIGKILL);
    }
}
```

**回答这个问题**：为什么 `kill(pid, SIGTERM)` 可能杀不死，而 `kill(pid, SIGKILL)` 一定能杀死？
（提示：SIGTERM 可以被进程捕获和处理，SIGKILL 是内核直接杀，进程没机会反应）

### 3.3 热身练习 2：进程组

```cpp
// 如果你的子进程又创建了孙子进程，怎么一起杀？
// 答案：setpgid(0, 0) - 创建独立进程组，然后 killpg(pgid, SIGTERM)
// 自己写一个验证：fork → 子进程再 fork 孙子 → 父进程 killpg 看孙子是否一起被杀
```

### 3.4 热身练习 3：读 /proc 计算 CPU 使用率

```
/proc/stat 第一行: cpu  user nice system idle iowait irq softirq steal
/proc/[pid]/stat: pid (comm) state ... utime stime cutime cstime ... rss ...

CPU% = (进程的 (utime+stime+cutime+cstime) 差值) / (系统总 CPU 时间差值) × 100%
```

动手写一个 `print_cpu(pid)` 函数，每秒打印一次目标进程的 CPU 使用率。

### 3.5 Day 3 项目任务

写出 `ProcStats.h/cpp` 和 `PerfProfiler.h/cpp`（放在 `cpp/common/` 下）：

**ProcStats**：
- `Snapshot(pid)` → 返回 `ProcSnapshot`（包含 utime, stime, rss, read_bytes, write_bytes, 系统总 ticks）
- `ComputeDelta(prev, curr)` → 返回 `ProcDelta`（CPU%, RSS KB, 读写 KB/s）

**PerfProfiler**（实现 `IProfiler` 接口）：
- `record(cfg)` → fork + execvp 跑 perf record，超时 killpg
- 注意：**请用 `execvp` 而非 `system()`**，为什么？写出你的理解

---

## 第 4 天：搭建 Agent —— 把前三天学到的串起来

### 4.1 Agent 的两个线程

```
Agent
├── HeartbeatChannel 线程: 每 5s 调 HealthCheck.Do()
│   ├── 携带 PidStats（读 /proc 自监控）
│   └── 收到 TaskDesc → 投递到 HotmethodChannel 的队列
│
└── HotmethodChannel 线程: 从队列取 TaskDesc
    ├── 根据 profiler_type 创建采集器
    ├── 执行 record()
    └── NotifyResult() 回报 Server
```

### 4.2 Day 4 项目任务

创建 `cpp/agent/` 下的文件：

1. **Config.h/cpp**：从 JSON 文件加载配置
   - 最简单的方式：用 `nlohmann/json` 库，5 行代码就能解析
   ```cpp
   #include <nlohmann/json.hpp>
   std::ifstream f("config.json");
   nlohmann::json j = nlohmann::json::parse(f);
   auto server_ips = j["server_ips"].get<std::vector<std::string>>();
   ```

2. **HeartbeatChannel.h/cpp**：心跳线程
   - 关键：用你 Day2 写的 `std::condition_variable` 和 `std::atomic<bool>` 管理线程生命周期
   - 每 5 秒调一次 gRPC HealthCheck::Do()
   - 收到 task_desc 后投递给 HotmethodChannel

3. **HotmethodChannel.h/cpp**：任务执行线程
   - 用你的 SafeQueue（或直接用 `std::queue + mutex + cv`）
   - 执行完后调 Hotmethod::NotifyResult()

4. **main.cpp**：Agent 入口
   - 解析命令行参数（`getopt_long`）
   - 加载配置
   - 启动两个线程
   - 处理 SIGINT/SIGTERM 信号优雅退出

### 4.3 可能会卡住的地方

**问题 1**：gRPC 的 stub 不是线程安全的，每个线程应该用自己的 stub 吗？

答案：`grpc::Channel` 是线程安全的，`Stub` 也是。多个线程可以共用一个 stub 并发调用。但每个调用需要独立的 `ClientContext`。

**问题 2**：子进程会继承父进程的 fd（包括 gRPC 的 socket），为什么这是问题？

fork 后子进程如果不关闭 socket fd，当父进程（Server）重启时，子进程仍然持有旧 socket 的引用，导致端口不释放。解决方案：`for (int fd = 3; fd < max_fd; fd++) close(fd);`

---

## 第 5 天：搭建 Server —— gRPC 服务的实现

### 5.1 Server 的结构

```
drop_server (一个进程，3 个 gRPC Service)
├── HealthCheckService::Do()
│   → 更新 Agent 在线状态
│   → 从 TaskQueue 取一个任务捎带下发
│
├── ControlService::CreateTask()
│   → 把 TaskDesc 放入对应 IP 的 TaskQueue
│
└── HotmethodService::NotifyResult()
    → 记录任务结果
```

### 5.2 Day 5 项目任务

创建 `cpp/server/` 下的文件：

1. **HealthCheckService.h/cpp**：
   - 维护一个 `map<ip, AgentRecord>`（含 hostname, is_online, last_heartbeat）
   - `Do()` 的逻辑：更新心跳时间 → TryPop 任务 → 有就塞进 response

2. **ControlService.h/cpp**：
   - `CreateTask()`：检查 Agent 在线 → PushBack 进队列

3. **HotmethodService.h/cpp**：
   - `NotifyResult()`：记录结果到 map

4. **main.cpp**：
   - 启动 gRPC Server，注册 3 个 Service
   - 启动离线检测线程（每 10 秒扫描，30 秒无心跳的判离线）

### 5.3 设计问题（写进你的设计文档）

1. 为什么需要离线检测线程？光靠心跳响应不够吗？
2. Server 的 `tasks_` map 和 `agents_` map 各自需要独立的 mutex 还是共用一个？为什么？

---

## 第 6 天：API Server + Analyzer + 前端

C++ 核心写完后，用 Python 快速搭建外围。

### 6.1 API Server（Flask，约 150 行）

**你先手动跑通 gRPC → 再用 Python 调 gRPC**：

```bash
# 用 grpcurl 验证 gRPC 接口（不需要写代码）
grpcurl -plaintext localhost:50051 list                    # 列出所有服务
grpcurl -plaintext localhost:50051 drop.control.Control/ListAgents  # 调 ListAgents
```

然后写 Flask 代码，核心就这几个路由：
- `POST /api/v1/tasks` → 写 SQLite + 调 gRPC CreateTask
- `GET /api/v1/tasks` → 读 SQLite
- `GET /api/v1/agents` → 读 SQLite（Agent 心跳时更新）
- `POST /api/v1/agent/heartbeat` → HTTP 版心跳（你 C++ Agent 调这个或 gRPC 二选一）

### 6.2 Analyzer（约 100 行 Python）

三个步骤，每步一个 subprocess：
```python
# 1. perf script → 文本
subprocess.run(["perf", "script", "-i", "perf.data"], stdout=open("out.txt","w"))

# 2. 折叠调用栈（这是核心逻辑，你自己写，不要调 stackcollapse-perf.pl）
#    输入: perf script 输出
#    输出: func_a;func_b;func_c 1234

# 3. 生成 SVG（Google flamegraph.pl，下载下来直接用）
subprocess.run(["perl", "flamegraph.pl", "collapsed.txt"], stdout=open("flame.svg","w"))
```

**必须自己写的是第 2 步**——折叠调用栈。理解 perf script 的输出格式，解析它。

### 6.3 前端（纯 HTML，不引入 React）

用我 README 里的架构图做参考，写出三个页面就够了：
- 首页：Agent 列表 + 任务列表 + 新建表单
- 任务详情：火焰图展示（`<object>` 标签嵌入 SVG）
- 轮询：JS `setInterval` 3 秒刷新一次任务状态

---

## 第 7 天：Docker + 测试 + 文档

### 7.1 Docker 化

**原则：评审者 `git clone && docker compose up` 必须 10 分钟内跑通。**

Dockerfile 的关键：
- 第一阶段（builder）：编译 C++ 代码
- 第二阶段（runtime）：只复制二进制 + Python 代码，尽可能小

docker-compose.yml 的关键：
- `drop_agent` 必须 `privileged: true` + `pid: host` 才能用 perf
- `network_mode: host` 让 Agent 能访问主机的 gRPC 端口

### 7.2 测试

C++ 测试（GTest）：
- TaskQueue 的 6 个测试（push/pop/fifo/push_front/空队列/pending_count）
- ProcStats 的 3 个测试（读自身/无效 PID/delta 计算）

端到端集成测试（Python，≥ 3 个）：
- 正常路径：创建任务 → 等 DONE → 查结果
- 异常路径 1：不存在的 PID
- 异常路径 2：Agent 离线时创建任务

### 7.3 设计文档（≤ 10 页）

必须包含：
1. **架构图**（手绘或 draw.io）
2. **状态机迁移图**（PENDING → RUNNING → UPLOADING → DONE/FAILED）
3. **关键决策**（为什么 C++? 为什么 gRPC? 为什么心跳捎带?）
4. **取舍说明**（什么做得好，什么妥协了，为什么）
5. **AI 协作章节**（你怎么用 AI 辅助开发的，AI 帮了什么、AI 哪里不行）
6. **性能自证**（心跳延迟、perf 启动时间、火焰图生成时间）
7. **如果再给我 7 天**（eBPF? 智能归因? React? 分布式? 给出具体方案）

---

## 📖 推荐学习资源

| 主题 | 资源 | 时间 |
|------|------|------|
| gRPC C++ | [gRPC 官方 tutorial](https://grpc.io/docs/languages/cpp/basics/) | 2h |
| C++ 多线程 | [cppreference: std::thread](https://en.cppreference.com/w/cpp/thread/thread) | 1h |
| fork/exec | `man 2 fork`, `man 3 exec` | 30min |
| /proc 文件系统 | `man 5 proc` 搜索 `/proc/[pid]/stat` | 30min |
| 火焰图 | [Brendan Gregg 的博客](https://www.brendangregg.com/flamegraphs.html) | 30min |
| Docker | [Docker Get Started](https://docs.docker.com/get-started/) | 2h |

---

## ❓ 常见困惑解答

**Q1: 为什么 proto 文件放在独立的 `proto/` 目录？**
A: 这 5 个文件是整个系统的"合同"。C++ Server 用、C++ Agent 用、Python API Server 用——所有人读同一份合同。实际项目中 proto 通常是独立 git repo。

**Q2: 为什么分 4 个 gRPC Service 而不是写一个大 Service？**
A: 单一职责。HealthCheck 归 HealthCheck，Task 归 Hotmethod——修改心跳逻辑不会影响任务下发逻辑。

**Q3: Agent 为什么不直接提供 HTTP 接口让 Server 调？**
A: 因为 Agent 可能在 NAT 后面（公司内网），Server 无法主动连 Agent。所以 Agent 主动心跳 + Server 捎带下发（这叫 "piggyback" 或 "long polling"）。

**Q4: 我应该从哪里开始写第一行代码？**
A: Day 1 的 echo server/client。写完它，gRPC 的概念就通了。然后从 proto 文件出发，逐个实现 Service。

---

**下一步**：去配 Linux 环境，然后从 Day 1 的 echo server 开始。遇到问题直接问我。
