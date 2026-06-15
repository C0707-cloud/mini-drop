# Mini-Drop 学习笔记

## Day 1：Proto 文件理解

### 问题 1：HealthCheckResponse 为什么嵌套 hotmethod.TaskDesc？省了什么？

因为 Agent 通常部署在内网（NAT/防火墙后面），Server 无法主动连接 Agent。
Agent 每 5 秒主动发起心跳请求，Server 在心跳**响应**里直接把待执行的任务塞回去。

这个设计叫 **piggyback（捎带）**。

省了什么：
- 省了一次 RPC 往返（Agent 不需要先问"有任务吗？"，再问"什么任务？"）
- 简化了状态管理——不会出现"Server 说有任务但 Agent 还没取"的中间状态
- 降低了延迟（对于性能分析场景，任务下发越快越好）

### 问题 2：RecordArgv 的每个字段对应 perf record 命令的哪个参数？

| RecordArgv 字段 | perf record 参数 | 含义 |
|:--|:--|:--|
| hz = 99 | -F 99 | 每秒采样 99 次 |
| duration_sec = 10 | sleep 10 | 采集 10 秒后停止 |
| pid = 1234 | -p 1234 | 只采集 PID=1234 的进程 |
| callgraph = "fp" | --call-graph fp | 用 frame pointer 展开调用栈 |
| subprocess = true | （逻辑上追踪所有线程） | 是否采集子进程 |
| event = "cpu-cycles" | -e cpu-cycles | 采集 CPU 周期事件 |

完整命令行：perf record -F 99 -p 1234 --call-graph fp -e cpu-cycles -g -- sleep 10

### 问题 3：NotifyResult 为什么同时有 File file 和 string cos_key？

大小分治策略：

- **小文件（<1MB）**：用 File file，直接把二进制内容内联在 gRPC 消息里，一个 RPC 搞定
- **大文件（≥1MB）**：Agent 先用 CosConfig 凭证把文件上传到对象存储（S3/COS），
  然后把 S3 路径填进 cos_key 告诉 Server

这样设计的原因：
- gRPC 默认消息大小限制 4MB，几百 MB 的 perf.data 会超限
- 小文件如果每次也走 COS，多一次 HTTP 往返，浪费
- 两套机制保证了灵活性和性能的平衡

---

## Proto import 关系图

```
                    common.proto
                   ↙      ↓      ↘
          init.proto   healthcheck.proto   hotmethod.proto
                       ↙          ↘
              (依赖于 hotmethod，  (依赖于 common + hotmethod)
               为了拿 TaskDesc)     control.proto
```

## Service 调用方向

| Service | RPC 方法 | 调用方向 |
|---------|---------|---------|
| Init | RegisterAgent | Agent → Server |
| Init | FetchConfig | Agent → Server |
| HealthCheck | Do | Agent → Server（每5秒） |
| Hotmethod | Collect | Server → Agent（紧急推送，不常用） |
| Hotmethod | NotifyResult | Agent → Server（采集完成上报） |
| Control | CreateTask | API Server → drop_server |
| Control | FetchData | API Server → drop_server |
| Control | StatAgent | API Server → drop_server |
| Control | ListAgents | API Server → drop_server |
