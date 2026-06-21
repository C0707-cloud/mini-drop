#!/usr/bin/env python3
# ============================================================
# Mini-Drop Analyzer — 火焰图 + TopN + 规则建议引擎
#
# 输入：perf.data 文件路径
# 输出：flamegraph.svg, top.json, suggestions.md
#
# 对应 PLAN.md 5.2
# ============================================================

import argparse
import os
import re
import subprocess
import json
import sys
from collections import Counter

# ============================================================
# 第 1 步：perf script → 文本调用栈
# ============================================================

def run_perf_script(perf_data_path):
    """调用 perf script 把 perf.data 转成可读文本"""
    result = subprocess.run(
        ["perf", "script", "-i", perf_data_path],
        capture_output=True,
        text=True
    )
    if result.returncode != 0:
        raise RuntimeError(f"perf script 失败: {result.stderr}")
    return result.stdout


# ============================================================
# 第 2 步：手写折叠调用栈（不调 stackcollapse-perf.pl）
#
# perf script 输出的格式：
#   myprog 12345 [000] 12345.678: cpu-clock:
#       401000 main+0x10 (/usr/bin/myprog)
#       402000 foo+0x20 (/usr/bin/myprog)
#       403000 bar+0x30 (/usr/bin/myprog)
#
# 折叠后格式（火焰图标准输入）：
#   main;foo;bar 1
# ============================================================

def collapse_stacks(perf_script_output):
    """自写栈折叠：解析 perf script 文本 → 折叠格式"""
    stacks = []
    current_stack = []
    current_pid   = None

    for line in perf_script_output.splitlines():
        # 跳过空行和元数据行
        line = line.strip()
        if not line:
            # 遇到空行 = 一个采样点结束 → 保存当前栈
            if current_stack:
                stacks.append(list(current_stack))
            current_stack = []
            current_pid   = None
            continue

        # 跳过以 # 开头的注释
        if line.startswith("#"):
            continue

        # 检测新的事件行（包含进程名和 PID）
        # 格式: process_name pid [cpu] timestamp: event:
        if line[0].isalpha() and not line.startswith("\t"):
            if current_stack:
                stacks.append(list(current_stack))
            current_stack = []
            continue

        # 调用栈行（以 tab 开头）
        if line.startswith("\t"):
            # 提取函数名: \t 401000 func+offset (binary)
            match = re.match(r"\s*[0-9a-f]+\s+(\S+)", line)
            if match:
                func = match.group(1)
                current_stack.append(func)

    # 最后一个栈
    if current_stack:
        stacks.append(list(current_stack))

    # 折叠：反转栈（perf 是从里到外，火焰图要外到里）+ 统计
    collapsed = Counter()
    for stack in stacks:
        if not stack:
            continue
        # 反转栈方向，用分号连接
        stack.reverse()
        key = ";".join(stack)
        collapsed[key] += 1

    return collapsed


# ============================================================
# 第 3 步：生成火焰图 SVG
# ============================================================

def generate_flamegraph(collapsed, output_svg):
    """把折叠栈喂给 flamegraph.pl 生成 SVG"""
    # 把折叠栈写成临时文件
    collapsed_text = "\n".join(
        f"{stack} {count}" for stack, count in collapsed.items()
    )

    # 尝试找 flamegraph.pl（从当前目录、系统 PATH、下载）
    script_paths = [
        "./flamegraph.pl",
        os.path.join(os.path.dirname(__file__), "flamegraph.pl"),
        "/usr/local/bin/flamegraph.pl",
        "flamegraph.pl"
    ]

    flamegraph_pl = None
    for p in script_paths:
        if os.path.exists(p):
            flamegraph_pl = p
            break

    if flamegraph_pl is None:
        # 自动下载
        import urllib.request
        url = "https://raw.githubusercontent.com/brendangregg/FlameGraph/master/flamegraph.pl"
        flamegraph_pl = os.path.join(os.path.dirname(__file__), "flamegraph.pl")
        print(f"[Analyzer] 下载 flamegraph.pl → {flamegraph_pl}")
        urllib.request.urlretrieve(url, flamegraph_pl)
        os.chmod(flamegraph_pl, 0o755)

    result = subprocess.run(
        ["perl", flamegraph_pl],
        input=collapsed_text,
        capture_output=True,
        text=True
    )

    if result.returncode != 0:
        raise RuntimeError(f"flamegraph.pl 失败: {result.stderr}")

    with open(output_svg, "w", encoding="utf-8") as f:
        f.write(result.stdout)

    return output_svg


# ============================================================
# 第 4 步：TopN 热点函数
# ============================================================

def compute_topn(collapsed, n=20):
    """统计热点函数"""
    func_counter = Counter()
    for stack, count in collapsed.items():
        # 只取栈的最后一个函数（最热的叶子节点）
        funcs = stack.split(";")
        if funcs:
            leaf = funcs[-1]
            func_counter[leaf] += count

    return func_counter.most_common(n)


# ============================================================
# 第 5 步：规则匹配建议引擎
# ============================================================

RULES = [
    {
        "regex": r".*malloc.*|.*_Znwm.*|operator new.*",
        "advice": "检测到频繁内存分配，建议使用对象池或 jemalloc 替代默认 malloc",
        "category": "内存"
    },
    {
        "regex": r".*memcpy.*|.*memset.*|.*memmove.*",
        "advice": "检测到大量内存拷贝操作，检查是否可通过零拷贝技术优化",
        "category": "内存"
    },
    {
        "regex": r".*pthread_mutex_lock.*|.*std::mutex.*|.*__GI___pthread_mutex_lock.*",
        "advice": "检测到锁竞争热点，建议检查锁粒度、考虑无锁数据结构或 RCU",
        "category": "并发"
    },
    {
        "regex": r".*lock.*|.*Lock.*|.*spin.*",
        "advice": "检测到锁定相关热点，确认是否存在不必要的锁竞争",
        "category": "并发"
    },
    {
        "regex": r".*write.*|.*send.*|.*__libc_write.*|.*__GI___libc_write.*",
        "advice": "检测到大量写操作，建议考虑写缓冲合并或异步 I/O",
        "category": "IO"
    },
    {
        "regex": r".*read.*|.*recv.*|.*__libc_read.*|.*__GI___libc_read.*",
        "advice": "检测到大量读操作，检查是否缺少缓存层或预读策略",
        "category": "IO"
    },
    {
        "regex": r".*strcmp.*|.*strlen.*|.*strcpy.*",
        "advice": "检测到字符串操作热点，建议使用 std::string_view 避免拷贝",
        "category": "优化"
    },
    {
        "regex": r".*__k.*vfs_.*|.*blk_.*|.*ext4_.*|.*xfs_.*",
        "advice": "检测到文件系统/IO 内核栈，可能为磁盘瓶颈，建议检查 IOPS",
        "category": "内核"
    },
    {
        "regex": r".*schedule.*|.*__schedule.*|.*do_task_dead.*",
        "advice": "检测到频繁调度，可能存在大量上下文切换，检查线程数量",
        "category": "内核"
    },
    {
        "regex": r".*__lll_lock_wait.*|.*futex.*|.*sys_futex.*",
        "advice": "检测到 futex 等待，高概率存在锁争用，建议使用 perf lock 分析",
        "category": "并发"
    },
]

def match_rules(topn):
    """TopN 函数 → 匹配规则 → 生成建议列表"""
    suggestions = []
    for func_name, count in topn:
        for rule in RULES:
            if re.search(rule["regex"], func_name):
                suggestions.append({
                    "function": func_name,
                    "count": count,
                    "advice": rule["advice"],
                    "category": rule["category"]
                })
                break  # 一个函数只匹配第一条规则
    return suggestions


# ============================================================
# 命令行入口
# ============================================================

def main():
    parser = argparse.ArgumentParser(description="Mini-Drop Analyzer")
    parser.add_argument("--input",  "-i", required=True, help="perf.data 文件路径")
    parser.add_argument("--output", "-o", default="flamegraph.svg", help="输出 SVG 路径")
    parser.add_argument("--topn",   "-n", type=int, default=20, help="TopN 数量")
    parser.add_argument("--topjson", default="top.json", help="TopN JSON 输出路径")
    parser.add_argument("--suggestions", default="suggestions.md", help="建议输出路径")
    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"错误: 文件不存在 {args.input}", file=sys.stderr)
        sys.exit(1)

    print(f"[Analyzer] 输入: {args.input}")

    # Step 1: perf script
    print("[Analyzer] 运行 perf script...")
    script_output = run_perf_script(args.input)

    # Step 2: 折叠调用栈
    print("[Analyzer] 折叠调用栈...")
    collapsed = collapse_stacks(script_output)
    print(f"[Analyzer] 折叠完成: {len(collapsed)} 个独立栈")

    # Step 3: 火焰图 SVG
    print("[Analyzer] 生成火焰图...")
    generate_flamegraph(collapsed, args.output)
    print(f"[Analyzer] 火焰图: {args.output}")

    # Step 4: TopN
    topn = compute_topn(collapsed, args.topn)
    with open(args.topjson, "w", encoding="utf-8") as f:
        json.dump([{"func": fn, "count": c} for fn, c in topn], f,
                  ensure_ascii=False, indent=2)
    print(f"[Analyzer] Top{args.topn}: {args.topjson}")

    # Step 5: 规则建议
    suggestions = match_rules(topn)
    with open(args.suggestions, "w", encoding="utf-8") as f:
        f.write("# Mini-Drop 分析建议\n\n")
        if suggestions:
            for s in suggestions:
                f.write(f"- **[{s['category']}]** `{s['function']}` ({s['count']}次)\n")
                f.write(f"  > {s['advice']}\n\n")
        else:
            f.write("未匹配到已知模式的热点函数。\n")
    print(f"[Analyzer] 建议: {args.suggestions} ({len(suggestions)} 条)")

    print("[Analyzer] 分析完成")


if __name__ == "__main__":
    main()
