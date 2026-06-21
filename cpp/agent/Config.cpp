#include "Config.h"

#include <fstream>
#include <cstdlib>
#include <nlohmann/json.hpp>

AgentConfig Config::LoadFromFile(const std::string& path) {
    AgentConfig cfg;
    std::ifstream f(path);
    if (!f) {
        throw std::runtime_error("无法打开配置文件: " + path);
    }

    nlohmann::json j = nlohmann::json::parse(f);

    cfg.agent_id      = j.value("agent_id", "");
    cfg.hostname      = j.value("hostname", "localhost");
    cfg.ip_addr       = j.value("ip_addr", "127.0.0.1");
    cfg.agent_version = j.value("agent_version", "1.0.0");

    cfg.server_ips    = j.value("server_ips",
                          std::vector<std::string>{"127.0.0.1:50051"});

    cfg.heartbeat_interval_sec = j.value("heartbeat_interval_sec", 5);
    cfg.task_timeout_sec       = j.value("task_timeout_sec", 60);

    cfg.supported_profilers = j.value("supported_profilers",
                                 std::vector<std::string>{"perf"});

    return cfg;
}

void Config::ApplyEnvOverrides(AgentConfig& cfg) {
    const char* env = nullptr;

    env = std::getenv("AGENT_ID");
    if (env) cfg.agent_id = env;

    env = std::getenv("AGENT_HOSTNAME");
    if (env) cfg.hostname = env;

    env = std::getenv("AGENT_IP");
    if (env) cfg.ip_addr = env;

    env = std::getenv("SERVER_URL");
    if (env) cfg.server_ips = {env};
}