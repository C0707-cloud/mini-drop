# Mini-Drop 7 天计划：学习 × 开发 × 交付物

> 每天都有三个产出：**学到的知识** + **写的代码** + **交付物进展**

---

## 总览：7 天如何覆盖全部交付物

```
                D1      D2      D3      D4      D5      D6      D7
Git 仓库         ████████████████████████████████████████████████████  (每天 commit)
docker compose                                                       ████
设计文档                ██      ██      ██      ██      ████████████
演示视频                                                             ████████
评测报告                                                      ████████
基础能力①       ██      ████    ████████                          
基础能力②                       ██      ████████                  
基础能力③                                       ██      ████      
基础能力④               ██      ████████████████                  
基础能力⑤                       ██      ████████                  
基础能力⑥       ████████████████████████████████████████████████████
扩展能力                                                        ████████
加分项                                                   ██      ████████
```

---

## Day 1：让 gRPC 跑通 + 理解系统契约

### 今天对应的项目要求
- 基础能力① 的通信基础（Server ↔ Agent 怎么对话）
- 交付物① 的第一次 commit

### 学习目标
- [ ] 理解 RPC 是什么，和 HTTP 的区别
- [ ] 能读懂 .proto 文件，知道 service / message / rpc 分别是什么
- [ ] 手写一个 gRPC Echo：Server 收到消息，返回 "Echo: xxx"
- [ ] 理解 protoc 编译流程：.proto → .pb.h/.pb.cc → 你的代码

### 具体任务

#### 1.1 搭环境（30 分钟）
```bash
# WSL2 Ubuntu 22.04
sudo apt update && sudo apt install -y build-essential cmake git g++ \
    libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc
```

#### 1.2 gRPC Echo（90 分钟）
在 `~/grpc-lab/` 下：
- 写 `echo.proto`（一个 service，一个 rpc）
- 手写 `protoc` 命令编译（不用 CMake，理解每一步）
- 写 `server.cpp`（继承生成的 Service 类，实现方法）
- 写 `client.cpp`（创建 Stub，远程调用）
- 手写 `g++` 编译 + 跑通
1.创建proto文件 
2.启动sever

> **为什么先做 Echo 而不是直接写 Mini-Drop？**
> 因为 Mini-Drop 有 4 个 Service、跨 3 个进程、涉及多线程——如果你连一个 Echo 都跑不通，后面一定崩。Echo 是整个项目的"最小可运行单元"。

#### 1.3 读懂 5 个 proto（30 分钟）
拿笔在纸上画出：5 个 proto 之间谁 import 了谁？每个 service 的调用方向是谁调谁？

```
healthcheck.proto ──import──→ hotmethod.proto ──import──→ common.proto
       │                            │
       │                            ▼
       │                       control.proto ──import──→ hotmethod.proto
       │
       ▼
   init.proto ──import──→ common.proto
```

回答三个问题写到 `notes.md`：
1. `HealthCheckResponse` 为什么嵌套 `hotmethod.TaskDesc`？省了什么？
2. Agent 从启动到执行完一个任务，经历了哪些 gRPC 调用？（按时间顺序列出）
3. `RecordArgv` 的每个字段对应 perf record 命令的哪个参数？

#### 1.4 初始化 Git 仓库（15 分钟）
```bash
cd ~/mini-drop   # 把项目移到 WSL 里
git init
git add proto/ README.md LEARN.md PLAN.md cpp/CMakeLists.txt notes.md
git commit -m "初始化项目：协议定义与构建骨架

5 个 proto 文件定义了 Agent/Server/API 之间的完整接口契约：
- common: 通用类型 (PidStats, CosConfig)
- healthcheck: 心跳 + 捎带任务下发
- hotmethod: 核心采集任务 + 结果回报
- control: API Server → drop_server 管理接口
- init: Agent 注册 + 配置拉取"
```

### 今日产物
| 产物 | 状态 |
|------|------|
| `~/grpc-lab/echo.proto, server.cpp, client.cpp` | Echo 跑通 |
| `mini-drop/notes.md`（3 个问题的答案） | 已写 |
| `mini-drop/.git/` | 第一次 commit |

---

## Day 2：多线程 + 任务队列

### 今天对应的项目要求
- 基础能力④ 状态机（任务状态流转的数据结构基础）
- 基础能力⑤ 心跳机制（心跳线程与任务线程解耦）

### 学习目标
- [ ] `std::thread` 创建线程、`join()` 等待
- [ ] `std::mutex` + `std::lock_guard` 保护共享数据
- [ ] `std::condition_variable` 生产者-消费者
- [ ] `std::atomic<bool>` 线程安全标志

### 具体任务

#### 2.1 热身：线程安全队列（45 分钟）
单独写一个 `safe_queue.cpp`，不依赖任何外部库：
```cpp
// 要求：
// - push(item) 生产者用
// - pop() 消费者用，队列空时阻塞等待
// - 测试：一个线程每秒 push 一个数字，另一个线程 pop 并打印
```

#### 2.2 写 TaskQueue（60 分钟）
在 `cpp/server/TaskQueue.h` 中实现：
```cpp
// 不是单队列！是 map<target_ip, deque<TaskDesc>>
// 需求：
// 1. PushBack(ip, task) / PushFront(ip, task)
// 2. TryPop(ip) → optional<TaskDesc>（没有就 nullopt，不阻塞）
// 3. PendingCount(ip) / TotalPending()
// 4. 线程安全（所有公共方法加锁）

// 关键设计问题（写到 notes.md）：
// 为什么 TryPop 不阻塞，而热身练习的 pop 要阻塞？
// 答：心跳线程不能等——每 5 秒必须发一次心跳，
//    如果等任务等到天荒地老，Server 就判它离线了
```

#### 2.3 git commit
```
git add cpp/server/TaskQueue.h
git commit -m "实现线程安全任务队列

按 IP 分组管理待执行任务，心跳线程通过 TryPop 非阻塞获取。
PushFront 支持紧急任务插队。

设计要点：
- std::map<IP, deque> 按 Agent IP 隔离队列
- 所有公共方法加 std::lock_guard
- TryPop 返回 std::optional，心跳线程不会阻塞等待"
```

### 今日产物
| 产物 | 状态 |
|------|------|
| `safe_queue.cpp`（热身练习，放 `~/grpc-lab/`） | 跑通 |
| `cpp/server/TaskQueue.h` | 完成 |
| `notes.md` 追加：为什么 TryPop 不阻塞 | 已写 |
| 第 2 次 git commit | 完成 |

---

## Day 3：Linux 系统编程 → Agent 核心

### 今天对应的项目要求
- 基础能力② Agent 采集数据
- 基础能力⑤ 心跳实现

### 学习目标
- [ ] `fork()` 克隆进程 + `execvp()` 替换程序
- [ ] `waitpid()` 回收子进程
- [ ] `setpgid()` + `killpg()` 超时杀全家
- [ ] 读 `/proc/[pid]/stat` 和 `/proc/[pid]/io`

### 具体任务

#### 3.1 热身练习（每个 30 分钟，独立的小程序）

**练习 A：`fork_exec_demo.cpp`**
```
fork → 子进程 execvp("sleep", ["sleep", "10"]) → 父进程 waitpid 等它结束 → 打印退出码
```

**练习 B：`timeout_kill.cpp`**
```
在练习 A 基础上加超时：子进程 sleep 100，父进程等 3 秒→SIGTERM→等 2 秒→SIGKILL
关键：fork 之后在子进程里 setpgid(0,0)，父进程用 killpg 杀整个进程组
```

**练习 C：`read_proc.cpp`**
```
读 /proc/[pid]/stat，解析出 utime, stime, rss
两个快照（间隔 1 秒）做差，算出 CPU%
```

#### 3.2 写 Agent 的三个文件（120 分钟）

**`cpp/common/IProfiler.h`**（接口）：
```cpp
class IProfiler {
public:
    virtual ~IProfiler() = default;
    virtual const char* name() const = 0;
    virtual ProfilerResult record(const ProfilerConfig& cfg) = 0;
};
```

**`cpp/common/PerfProfiler.cpp`**（实现）：
- `BuildArgv(cfg)` → 把 RecordArgv 翻译成 `perf record -F 99 -g -p 1234 -o xxx -- sleep 10` 这样的参数数组
- `ExecuteWithTimeout(argv, timeout)` → fork + setpgid + execvp + 超时 killpg + waitpid
- `record(cfg)` → 组合上面两个

**`cpp/common/ProcStats.cpp`**（实现）：
- `Snapshot(pid)` → 读 /proc，返回结构体
- `ComputeDelta(prev, curr)` → 计算 CPU%、RSS、IO KB/s

#### 3.3 git commit
```
git add cpp/common/
git commit -m "实现采集器框架与 perf 采集器

IProfiler 策略模式：新增采集器只需实现接口，不改现有代码。
PerfProfiler 封装 fork/exec 生命周期：独立进程组 + 超时 killpg。
ProcStats 解析 /proc 文件系统计算 CPU/内存/IO 使用率。

安全设计：
- 拒绝 system()，使用 execvp 防 shell 注入
- 子进程关闭继承的 fd，防 socket 泄漏
- setpgid 创建独立进程组，超时 killpg 防孤儿进程"
```

### 今日产物
| 产物 | 状态 |
|------|------|
| `~/grpc-lab/fork_exec_demo.cpp` | 跑通 |
| `~/grpc-lab/timeout_kill.cpp` | 跑通 |
| `~/grpc-lab/read_proc.cpp` | 跑通 |
| `cpp/common/IProfiler.h` | 完成 |
| `cpp/common/PerfProfiler.h + .cpp` | 完成 |
| `cpp/common/ProcStats.h + .cpp` | 完成 |
| 第 3 次 git commit | 完成 |

---

## Day 4：Agent 组装 + Server 实现

### 今天对应的项目要求
- 基础能力① Agent 端完成
- 基础能力④ 状态机实现
- 基础能力⑤ 心跳 + 离线检测

### 具体任务

#### 4.1 写 Agent（90 分钟）

**`cpp/agent/Config.h + .cpp`**：
- 从 JSON 读配置（用 nlohmann/json）
- 环境变量覆盖（`AGENT_ID`, `SERVER_URL` 等）

**`cpp/agent/HeartbeatChannel.h + .cpp`**：
```
线程主循环:
  while(running) {
    读 /proc 拿自身 PidStats
    调 HealthCheck::Do() 发心跳
    if (response.has_task) {
      把 task 扔进 HotmethodChannel 的队列
    }
    sleep(5)
  }
```

**`cpp/agent/HotmethodChannel.h + .cpp`**：
```
线程主循环:
  while(running) {
    从队列取任务（阻塞等）
    创建对应的 IProfiler
    执行 record()
    调 Hotmethod::NotifyResult() 上报
  }
```

**`cpp/agent/main.cpp`**：
- 解析命令行参数（getopt_long）
- 加载配置
- 启动两个线程
- SIGINT/SIGTERM 优雅退出

#### 4.2 写 Server（90 分钟）

**`cpp/server/HealthCheckService.h + .cpp`**：
- 维护 `map<ip, AgentRecord>`（含 hostname, is_online, last_heartbeat）
- `Do()`：更新心跳 → TryPop 任务 → 捎带返回

**`cpp/server/ControlService.h + .cpp`**：
- `CreateTask()`：检查 Agent 在线 → PushBack

**`cpp/server/HotmethodService.h + .cpp`**：
- `NotifyResult()`：记录结果

**`cpp/server/main.cpp`**：
- 启动 gRPC Server
- 启动离线检测线程（每 10s 扫描，>30s 判离线 → 打审计日志）

#### 4.3 CMake 串起来

把 `CMakeLists.txt` 里的 TODO 逐个取消注释，把文件列表填对，确保 `cmake .. && make` 能编译出两个二进制。

#### 4.4 验证

```bash
# 终端1：启动 Server
./build/drop_server

# 终端2：启动 Agent（需要 config.json）
./build/drop_agent -c ../config.json.example

# 应该看到 Server 日志里出现 Agent 心跳
```

#### 4.5 git commit
```
git add cpp/agent/ cpp/server/ CMakeLists.txt
git commit -m "实现 Agent 双线程架构与 Server gRPC 服务

Agent:
- HeartbeatChannel: 独立心跳线程，每5秒向Server发送心跳
- HotmethodChannel: 独立工作线程，从队列取任务→创建采集器→执行→上报
- 支持多Server故障转移与自动重连

Server:
- HealthCheckService: 心跳处理+任务捎带下发
- ControlService: 任务创建入口
- HotmethodService: 结果收集
- 离线检测线程: 30秒无心跳判离线
- TaskQueue: 线程安全，按IP分组管理"
```

### 今日产物
| 产物 | 状态 |
|------|------|
| Agent 全部 7 个文件 | 完成 |
| Server 全部 7 个文件 | 完成 |
| CMakeLists.txt 完整 | 完成 |
| Server + Agent 心跳互通 | 验证通过 |
| 第 4 次 git commit | 完成 |

---

## Day 5：API Server + Analyzer + 前端

### 今天对应的项目要求
- 基础能力① Web UI 部分
- 基础能力③ Analyzer 火焰图
- 基础能力④ 状态机落库

### 具体任务

#### 5.1 API Server——Python Flask（60 分钟）
在 `apiserver/app.py` 中实现：
- SQLite 3 张表（tasks, agents, audit_log）
- `POST /api/v1/tasks` 创建任务
- `GET /api/v1/tasks` 任务列表
- `GET /api/v1/agents` Agent 列表
- `POST /api/v1/agent/heartbeat` HTTP 心跳
- 状态迁移时同时写 tasks 表和 audit_log 表

#### 5.2 Analyzer（45 分钟）
在 `analyzer/analyzer.py` 中实现：
- perf script → 文本调用栈
- 手写栈折叠逻辑（这是面试会问的，别调 stackcollapse-perf.pl）
- 调 flamegraph.pl 生成 SVG
- TopN 热点函数统计
- 规则匹配建议引擎（regex 匹配函数名 → 预置优化建议）

#### 5.3 前端（45 分钟）
在 `web_ui/templates/` 下写两个 HTML：
- 首页：Agent 卡片 + 新建任务表单 + 任务表格
- 详情页：基本信息 + 火焰图 SVG 嵌入

#### 5.4 端到端联调
```
浏览器 → Flask → gRPC → C++ Server → C++ Agent → perf
                 ↑_________↓
              Analyzer → flamegraph.svg
```

#### 5.5 git commit
```
git add apiserver/ analyzer/ web_ui/
git commit -m "实现 API Server、分析引擎与 Web 前端

API Server (Flask): HTTP→gRPC桥接 + SQLite + 审计日志
Analyzer: perf→火焰图SVG + TopN热点 + 规则建议引擎
Web UI: 任务管理 + Agent监控 + 火焰图展示"
```

### 今日产物
| 产物 | 状态 |
|------|------|
| `apiserver/app.py` | 完成 |
| `analyzer/analyzer.py` | 完成 |
| `web_ui/templates/index.html` | 完成 |
| `web_ui/templates/task_detail.html` | 完成 |
| 端到端链路跑通 | 验证通过 |
| 第 5 次 git commit | 完成 |

---

## Day 6：Docker 部署 + 测试 + 文档

### 今天对应的项目要求
- 基础能力⑥ 单测 + e2e 测试
- 基础能力⑦ docker compose up
- 交付物② 一键跑通
- 交付物③ 设计文档

### 具体任务

#### 6.1 C++ 单元测试（60 分钟）
`cpp/tests/test_taskqueue.cpp`：6 个测试用例
`cpp/tests/test_procstats.cpp`：3 个测试用例

目标：覆盖率 ≥ 50%

#### 6.2 集成测试（30 分钟）
`tests/test_e2e.py`：3 个端到端测试
- 正常路径：创建任务 → 等 DONE → 查结果
- 异常路径 1：不存在的 PID
- 异常路径 2：Agent 离线时创建任务

#### 6.3 Docker 化（60 分钟）
- `Dockerfile`：多阶段构建（builder 编译 C++ → runtime 只复制二进制）
- `docker-compose.yml`：4 个 service（server/agent/apiserver/analyzer）
- `Makefile`：`make demo` 一键启动

验证：在干净环境 `git clone && docker compose up`，10 分钟内见到火焰图。

#### 6.4 设计文档（90 分钟）
≤ 10 页，必须包含：
1. 架构图（组件 + 数据流 + 通信协议）
2. 状态机迁移图
3. 3 个关键设计决策（为什么 C++？为什么 gRPC？为什么心跳捎带？）
4. 取舍说明（做得好的是什么？妥协了什么？）
5. AI 协作章节（AI 帮了什么？哪里不行？怎么改 prompts？）
6. 性能自证（心跳延迟 < ?ms, perf 启动 < ?ms, 火焰图生成 < ?s）
7. 如果再给 7 天（eBPF 怎么做？智能归因怎么做？具体方案）

#### 6.5 git commit
```
git add Dockerfile docker-compose.yml Makefile cpp/tests/ tests/
git commit -m "完成Docker部署、单元测试与集成测试

Docker: 多阶段构建，docker compose up 一键启动
测试: GTest单元测试(9用例) + Python集成测试(3场景)
      覆盖率: C++ ≥ 50%, 端到端 3/3 通过"
```

### 今日产物
| 产物 | 状态 |
|------|------|
| 9 个 C++ 单元测试 | 通过 |
| 3 个 e2e 集成测试 | 通过 |
| Docker 一键部署 | 验证通过 |
| 设计文档 | 完成 |

---

## Day 7：演示视频 + 收尾

### 今天对应的项目要求
- 交付物④ 演示视频
- 交付物⑤ 评测报告（如选加分项）
- 交付物① 最终检查

### 具体任务

#### 7.1 录演示视频（≤ 15 分钟）

视频结构（照着录，不用剪辑）：
1. **(0-2 分钟)** docker compose up 启动 → 浏览器打开 → 看到 Agent 在线
2. **(2-5 分钟)** 新建 CPU 采集任务 → 实时看状态变化 → 火焰图出来
3. **(5-8 分钟)** 用 `dd`/`stress-ng` 制造 IO 抖动 → eBPF 采集（如选了扩展能力）
4. **(8-10 分钟)** 持续 profiling 演示（如选了扩展能力）
5. **(10-13 分钟)** 最得意设计：打开 TaskQueue.h 讲线程安全设计
6. **(13-15 分钟)** 如果重做我会怎么改

#### 7.2 智能归因评测报告（如选加分项）

结构：
- 测试场景（造什么问题 → LLM 分析什么数据 → 产出什么结论）
- 可验证性证明（LLM 的结论能落实到具体函数/行号吗？）
- 误判分析（多少个建议是对的？多少个是错的？）
- LLM 调用的工具定义（你定义了哪些工具？为什么是这些？）

#### 7.3 最终检查

```bash
# 1. 确保干净环境能跑
git clone <your-repo> test-clone && cd test-clone
docker compose up -d
# 等 2 分钟，浏览器打开 localhost:5000

# 2. 检查 commit 历史
git log --oneline
# 确认没有 "update" "fix" "wip" 这种无意义 message

# 3. 检查 README
# - 写了硬件/内核/权限要求吗？
# - 写了 docker compose up 步骤吗？
# - 写了 make demo 吗？
```

---

## 附录：知识点 → 项目需求 对照表

| 你学的知识点 | 用在项目的哪里 |
|-------------|--------------|
| gRPC unary RPC | 4 个 Service 的 7 个 RPC 方法 |
| .proto 编译 | proto/ 下 5 个文件 → C++/Python 代码生成 |
| std::thread | HeartbeatChannel 线程 + HotmethodChannel 线程 + 离线检测线程 |
| std::mutex | TaskQueue 线程安全、Agent 状态保护 |
| std::condition_variable | HotmethodChannel 等待任务队列 |
| fork + execvp | Agent 启动 perf 子进程 |
| waitpid + killpg | 超时杀进程、防僵尸 |
| /proc 解析 | ProcStats 自监控 |
| Flask 路由 | API Server HTTP 接口 |
| SQLite | 任务表 + Agent 表 + 审计日志表 |
| Docker 多阶段构建 | docker compose up 一键部署 |
| GTest | TaskQueue + ProcStats 单元测试 |
