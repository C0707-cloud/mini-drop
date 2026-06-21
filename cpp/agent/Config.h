#ifndef MINI_DROP_CONFIG_H
#define MINI_DROP_CONFIG_H

// ============================================================
// Config — Agent 配置加载器
//
// 从 JSON 文件读取配置，支持环境变量覆盖。
// ============================================================

#include <string>
#include <vector>

struct AgentConfig {
    std::string agent_id;
    std::string hostname;
    std::string ip_addr;
    std::string agent_version;

    std::vector<std::string> server_ips;    // Server 地址列表（故障转移）
    int heartbeat_interval_sec = 5;         // 心跳间隔
    int task_timeout_sec       = 60;        // 任务默认超时

    std::vector<std::string> supported_profilers; // 支持的采集器
};

class Config {
public:
    // 从 JSON 文件加载配置
    static AgentConfig LoadFromFile(const std::string& path);

    // 环境变量覆盖：AGENT_ID, AGENT_HOSTNAME, AGENT_IP, SERVER_URL
    static void ApplyEnvOverrides(AgentConfig& cfg);
};

#endif // MINI_DROP_CONFIG_H