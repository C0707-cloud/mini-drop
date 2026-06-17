#include "ProcStats.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

ProcSnapshot ProcStats::Snapshot(int pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream file(path);
    if (!file) return {};

    std::string line;
    std::getline(file, line);

    // 跳过进程名（括号包裹）
    size_t start = line.rfind(')');
    if (start == std::string::npos) return {};

    std::istringstream iss(line.substr(start + 2));
    std::string field;
    std::vector<std::string> fields;
    while (iss >> field) fields.push_back(field);

    ProcSnapshot snap;
    snap.utime = std::stoull(fields[11]);  // utime
    snap.stime = std::stoull(fields[12]);  // stime
    snap.rss   = std::stoull(fields[21]);  // rss

    // 读系统总 CPU 时间
    std::ifstream statFile("/proc/stat");
    std::getline(statFile, line);
    std::istringstream cpuStream(line);
    std::string ignore;
    cpuStream >> ignore;
    uint64_t val;
    uint64_t total = 0;
    while (cpuStream >> val) total += val;
    snap.total = total;

    return snap;
}

ProcDelta ProcStats::ComputeDelta(const ProcSnapshot& prev,
                                  const ProcSnapshot& curr) {
    uint64_t procDelta  = (curr.utime + curr.stime) - (prev.utime + prev.stime);
    uint64_t totalDelta = curr.total - prev.total;

    ProcDelta delta;
    delta.cpu_percent = 0.0;
    if (totalDelta > 0) {
        delta.cpu_percent = 100.0 * procDelta / totalDelta;
    }
    delta.rss_kb = curr.rss * 4;  // 1 页 = 4KB
    return delta;
}

