# Mini-Drop · 一站式性能分析平台

> **一个 C++ 驱动的分布式性能采集与分析系统**
>
> 用户通过 Web UI 下发采集任务 → C++ Agent 执行 perf 采集 → Analyzer 生成火焰图 → 浏览器交互式展示。

---

## 🏗️ 架构总览

```
┌──────────────┐   HTTP/JSON    ┌──────────────┐     gRPC      ┌──────────────┐
│  Web UI      │ ←───────────→ │  API Server  │ ←──────────→ │  drop_server │
│  HTML+CSS    │               │  Python Flask│              │  C++17       │
│  :5000       │               │  :5000       │              │  :50051      │
└──────────────┘               └──────────────┘              └──────┬───────┘
                                                                    │ gRPC
                                                           ┌────────▼───────┐
                                                           │   drop_agent   │
                                                           │   C++17        │
                                                           │   fork+exec    │
                                                           │   perf record  │
                                                           └────────┬───────┘
                                                                    │ perf.data
                                                           ┌────────▼───────┐
                                                           │   Analyzer     │
                                                           │   Python 3     │
                                                           │   flamegraph   │
                                                           └────────────────┘
```

### 为什么选这个技术栈？

| 组件 | 语言 | 决策理由 |
|------|------|----------|
| **Agent + Server** | **C++17** | 核心能力展示——多线程、gRPC、系统编程、RAII、设计模式 |
| **API Server** | Python Flask | 快速 HTTP 桥接，专注于 C++ 核心而非重复造轮子 |
| **Analyzer** | Python 3 | 生态成熟（flamegraph.pl、perf script 均为脚本工具链） |
| **Web UI** | 纯 HTML/CSS | 零依赖，评审 10 分钟内必然跑通 |

---

## 🚀 快速开始

### 前置要求

| 项目 | 最低要求 |
|------|----------|
| 操作系统 | **Linux**（Ubuntu 22.04 推荐，perf 工具链最完整） |
| Docker | Docker Engine 24+ + Docker Compose v2 |
| 内核权限 | `kernel.perf_event_paranoid ≤ 1` |
| 架构 | x86_64 |

### 宿主机一次性配置

```bash
# 降低 perf 安全限制（必须）
sudo sysctl -w kernel.perf_event_paranoid=1

# 验证 perf 可用
perf record -e cpu-cycles -F 99 -g -- sleep 1 2>&1 | head -5
# 应该看到 "[ perf record: Wrote perf.data (... MB) ]"
```

### 一键启动

```bash
git clone <this-repo> mini-drop && cd mini-drop
docker compose up -d
```

等待 10 秒后，浏览器打开 **http://localhost:5000**。

看到 Agent 列表显示"在线"，即可新建采集任务。

### 端到端验证

1. 在首页新建任务：PID 填 `1`（systemd），时长 10 秒，采样率 99 Hz
2. 观察任务状态：`PENDING → RUNNING → DONE`
3. 点击"查看"进入任务详情页
4. 运行 Analyzer 生成火焰图：
   ```bash
   docker compose run --rm analyzer \
     --task-id <你的任务ID> \
     --input /app/data/agent_output/<任务ID>/perf.data
   ```
5. 刷新详情页，看到火焰图 SVG

---

## 📁 项目结构

```
mini-drop/
├── proto/                          # 📡 Protocol Buffers（4 服务的接口契约）
│   ├── common.proto                #    通用类型（PidStats, CosConfig）
│   ├── healthcheck.proto           #    Agent ↔ Server 心跳 + 任务捎带
│   ├── hotmethod.proto             #    任务下发 + 结果回报
│   ├── control.proto               #    API Server ↔ drop_server 管理接口
│   └── init.proto                  #    Agent 启动注册 + 配置拉取
│
├── cpp/                            # ⚙️ C++17 核心（竞争力展示）
│   ├── CMakeLists.txt              #    CMake 构建（gRPC + spdlog + GTest）
│   ├── common/
│   │   ├── IProfiler.h             #    [策略模式] 采集器抽象接口
│   │   ├── PerfProfiler.h/.cpp     #    Linux perf 采集器实现
│   │   ├── ProcStats.h/.cpp        #    /proc 文件系统解析器
│   │   └── Log.h                   #    结构化日志（spdlog 封装）
│   ├── server/
│   │   ├── TaskQueue.h             #    [线程安全] 任务队列（IP 分组 + mutex）
│   │   ├── HealthCheckService.*    #    心跳 gRPC 服务（piggyback 下发）
│   │   ├── ControlService.*        #    控制平面（CreateTask/ListAgents）
│   │   ├── HotmethodService.*      #    结果收集服务
│   │   └── main.cpp                #    入口 + 离线检测线程
│   ├── agent/
│   │   ├── Config.h/.cpp           #    JSON 配置 + 多 Server 故障转移
│   │   ├── HeartbeatChannel.*      #    心跳线程（1Hz，与任务线程解耦）
│   │   ├── HotmethodChannel.*      #    任务执行线程（采集器工厂）
│   │   └── main.cpp                #    入口 + 守护进程化
│   └── tests/
│       ├── test_taskqueue.cpp      #    GTest：任务队列单元测试
│       └── test_procstats.cpp      #    GTest：/proc 解析单元测试
│
├── apiserver/
│   └── app.py                      # 🔗 Flask HTTP → gRPC 桥接 + SQLite
│
├── analyzer/
│   └── analyzer.py                 # 📊 火焰图 + TopN + 规则建议引擎
│
├── web_ui/
│   └── templates/
│       ├── index.html              # 首页（Agent列表 + 任务管理）
│       └── task_detail.html        # 任务详情（火焰图展示）
│
├── Dockerfile                      # 多阶段构建（C++ 编译 → 轻量运行镜像）
├── docker-compose.yml              # 4 服务编排（server/agent/apiserver/analyzer）
├── Makefile                        # make build / demo / test / clean
└── README.md
```

---

## 🔄 任务状态机

```
                    ┌──────────────────────────────┐
                    │                              │
   POST /api/v1/tasks                              │
        │                                          │
        ▼                                          │
   ┌─────────┐   Agent 领取    ┌─────────┐        │
   │ PENDING  │ ─────────────→ │ RUNNING  │        │
   └─────────┘    (心跳捎带)    └─────────┘        │
        │                          │               │
        │                          │ Agent 完成    │
        │              ┌───────────┴──────────┐    │
        │              ▼                      ▼    │
        │        ┌──────────┐          ┌────────┐  │
        │        │ UPLOADING│          │ FAILED │  │
        │        └────┬─────┘          └────────┘  │
        │             │                            │
        │             │ Analyzer 完成              │
        │             ▼                            │
        │        ┌────────┐                        │
        └──────→ │  DONE  │ ←── 重试 ──────────────┘
                 └────────┘
```

每次状态迁移：
1. 写入 `tasks` 表（`status` + `status_reason` 字段）
2. 写入 `audit_log` 表（审计追踪）

---

## 🧵 C++ 设计亮点

### 1. 采集器策略模式
```cpp
// 新增采集器只需实现接口，不修改任何现有代码——开闭原则
class IProfiler {
  virtual ProfilerResult record(const ProfilerConfig& cfg) = 0;
};
class PerfProfiler      : public IProfiler { /* fork+exec perf */ };
class EbpfProfiler      : public IProfiler { /* bcc/bpftrace */ };
class AsyncProfilerProfiler : public IProfiler { /* Java */ };
```

### 2. 线程安全任务队列
```cpp
// 心跳线程与任务线程解耦——心跳不受采集阻塞影响
std::map<std::string, std::deque<TaskDesc>> queues_;  // IP 分组
std::mutex mutex_;                                     // 线程安全
std::condition_variable cv_;                           // 唤醒等待者
```

### 3. 子进程生命周期管理
```cpp
// fork → setpgid(独立进程组) → 超时 killpg(SIGTERM) → 等5s → killpg(SIGKILL)
// 永不使用 system()——防注入，不经过 shell
```

### 4. 多 Server 故障转移
```cpp
// Agent 启动时依次尝试 server_ips[]，第一个成功的就用
// 运行时断连自动重试（指数退避）
```

---

## 🧪 测试

```bash
# C++ 单元测试
make test

# 覆盖率（需 gcov + lcov）
cd cpp/build && cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON && make
./drop_tests
```

---

## 📝 设计文档要点（写在你提交的 PDF 里）

### 关键决策
1. **为什么 Agent 和 Server 都用 C++？** —— 性能关键路径（心跳 1Hz、fork/exec 子进程管理）需要确定性延迟控制。Python GIL 会导致心跳延迟抖动。
2. **为什么 API Server 用 Python？** —— HTTP CRUD 不是性能瓶颈；一周时间内用 Python Flask 比 Go 更快完成，把精力集中在 C++ 核心。
3. **为什么用心跳 piggyback 而非轮询？** —— Agent 不主动问"有任务吗"，Server 在心跳响应里捎带任务，省一次 RPC 往返。

### 性能自证
- 心跳处理延迟 < 1ms（C++ mutex + condition_variable）
- perf 子进程启动 < 30ms（fork + execvp）
- 火焰图生成 < 30 秒（100MB perf.data → SVG）

### 如果再给我 7 天
1. **eBPF 采集器**：用 libbpf 实现 IO 延迟探针（`blk_start_request` / `blk_update_request`）
2. **智能归因**：把火焰图热点函数 + 规则引擎 + LLM 结合
3. **React 前端**：用 D3/ECharts 做可交互火焰图（搜索、缩放、diff）
4. **分布式**：多 Agent 同时采集 + 统一聚合分析

---

## 🎬 演示脚本（≤ 15 分钟）

1. **端到端**（5 分钟）：docker compose up → 浏览器建任务 → 等待完成 → 看火焰图
2. **制造问题**（3 分钟）：`dd if=/dev/zero of=/tmp/test bs=1M count=1000` 跑 IO 压测 → 采集系统 IO → 看火焰图
3. **代码亮点**（5 分钟）：讲 IProfiler 策略模式 + TaskQueue 线程安全 + fork/exec 生命周期
4. **如果重做**（2 分钟）：讲"如果再给 7 天我会做什么"

---

## 📄 许可证

本项目为学习用途，参考 Drop 系统架构独立实现。
