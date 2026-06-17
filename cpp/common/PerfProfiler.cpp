#include "PerfProfiler.h"

#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

// ----------------------------------------------------------
// BuildArgv：把配置翻译成 perf record 命令行
// ----------------------------------------------------------
std::vector<std::string> PerfProfiler::BuildArgv(const ProfilerConfig& cfg) {
    std::vector<std::string> argv;

    argv.push_back("perf");
    argv.push_back("record");

    argv.push_back("-F");
    argv.push_back(std::to_string(cfg.hz));

    argv.push_back("-p");
    argv.push_back(std::to_string(cfg.pid));

    argv.push_back("--call-graph");
    argv.push_back(cfg.callgraph.empty() ? "fp" : cfg.callgraph);

    argv.push_back("-e");
    argv.push_back(cfg.event.empty() ? "cpu-cycles" : cfg.event);

    argv.push_back("-g");   // 记录调用栈
    argv.push_back("-o");
    argv.push_back("/tmp/perf_" + std::to_string(cfg.pid) + ".data");

    argv.push_back("--");
    argv.push_back("sleep");
    argv.push_back(std::to_string(cfg.duration_sec));

    return argv;
}

// ----------------------------------------------------------
// ExecuteWithTimeout：fork + setpgid + execvp + 超时 killpg
// ----------------------------------------------------------
int PerfProfiler::ExecuteWithTimeout(const std::vector<std::string>& argv,
                                     int timeoutSec) {
    // 把 vector<string> 转成 char*[] 数组
    std::vector<char*> cargv;
    for (const auto& s : argv) {
        cargv.push_back(const_cast<char*>(s.c_str()));
    }
    cargv.push_back(nullptr);

    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "[PerfProfiler] fork 失败" << std::endl;
        return -1;
    }

    if (pid == 0) {
        // ===== 子进程 =====
        setpgid(0, 0);  // 独立进程组

        // 关闭多余 fd，防止 socket 泄漏
        for (int fd = 3; fd < 64; ++fd) {
            close(fd);
        }

        execvp(cargv[0], cargv.data());

        // execvp 失败才会走到这里
        std::cerr << "[PerfProfiler] execvp 失败: " << cargv[0] << std::endl;
        _exit(127);
    }

    // ===== 父进程 =====
    int status = 0;

    while (timeoutSec > 0) {
        sleep(1);
        timeoutSec--;

        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid) {
            // 子进程正常结束
            if (WIFEXITED(status)) return WEXITSTATUS(status);
            if (WIFSIGNALED(status)) return -WTERMSIG(status);
            return 0;
        }
    }

    // 超时：先 SIGTERM，等 5 秒，再 SIGKILL
    std::cerr << "[PerfProfiler] 超时，杀进程组" << std::endl;
    killpg(pid, SIGTERM);
    sleep(5);

    pid_t result = waitpid(pid, &status, WNOHANG);
    if (result == 0) {
        std::cerr << "[PerfProfiler] SIGTERM 无效，发 SIGKILL" << std::endl;
        killpg(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
    return -1;
}

// ----------------------------------------------------------
// Record：组合 BuildArgv + ExecuteWithTimeout
// ----------------------------------------------------------
ProfilerResult PerfProfiler::Record(const ProfilerConfig& cfg) {
    ProfilerResult result;
    result.success = false;

    auto argv = BuildArgv(cfg);

    // 打印命令（调试用）
    std::cout << "[PerfProfiler] 执行:";
    for (const auto& s : argv) {
        std::cout << " " << s;
    }
    std::cout << std::endl;

    int exitCode = ExecuteWithTimeout(argv, cfg.duration_sec + 10);

    if (exitCode == 0) {
        result.output_file = "/tmp/perf_" + std::to_string(cfg.pid) + ".data";
        result.success = true;
    } else if (exitCode == -1) {
        result.error = "采集超时或被杀";
    } else if (exitCode == 127) {
        result.error = "perf 未安装或 execvp 失败";
    } else {
        result.error = "perf 退出码: " + std::to_string(exitCode);
    }

    return result;
}

