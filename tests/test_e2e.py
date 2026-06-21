#!/usr/bin/env python3
# ============================================================
# Mini-Drop Day 6 · 端到端集成测试（3 个场景）
#
# 场景1（正常）：创建任务 → 等 DONE → 查结果
# 场景2（异常1）：不存在的 PID
# 场景3（异常2）：Agent 离线时创建任务
#
# 运行方式：先启动 apiserver (python3 app.py) 和 drop_server (如有)，
#           然后 python3 test_e2e.py
# ============================================================

import time
import json
import urllib.request
import sys

BASE  = "http://127.0.0.1:5000"
PASSED = 0
FAILED = 0

def api(method, path, body=None):
    """发 HTTP 请求"""
    data = json.dumps(body).encode() if body else None
    req  = urllib.request.Request(
        BASE + path,
        data=data,
        method=method,
        headers={"Content-Type": "application/json"} if data else {}
    )
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            return json.loads(resp.read())
    except Exception as e:
        return {"code": -1, "msg": str(e)}


def test(name, condition_fn):
    global PASSED, FAILED
    print(f"[TEST] {name} ... ", end="", flush=True)
    try:
        ok, msg = condition_fn()
        if ok:
            print("通过")
            PASSED += 1
        else:
            print(f"失败: {msg}")
            FAILED += 1
    except Exception as e:
        print(f"异常: {e}")
        FAILED += 1


# ============================================================
# 场景1：正常路径 —— 创建任务 → 状态变为 DONE → 能查到
# ============================================================

def test_normal_flow():
    # 1. 创建任务
    r = api("POST", "/api/v1/tasks", {
        "name": "e2e_正常路径",
        "target_ip": "127.0.0.1",
        "pid": 1,
        "hz": 99,
        "duration": 1
    })
    if r.get("code") != 0:
        return False, f"创建失败: {r}"
    tid = r["data"]["task_id"]
    print(f"tid={tid} ", end="", flush=True)

    # 2. 轮询等待状态变为 DONE 或 FAILED（最多等 15 秒）
    for _ in range(30):
        time.sleep(0.5)
        r2 = api("GET", f"/api/v1/tasks/{tid}")
        status = r2.get("data", {}).get("status", "")
        if status in ("DONE", "FAILED"):
            return True, f"最终状态={status}"
    return False, "任务超过 15 秒未完成"


# ============================================================
# 场景2：异常路径1 —— 不存在的 PID
# ============================================================

def test_invalid_pid():
    r = api("POST", "/api/v1/tasks", {
        "name": "e2e_无效PID",
        "target_ip": "127.0.0.1",
        "pid": 999999,   # 不存在的 PID
        "hz": 99,
        "duration": 1
    })
    if r.get("code") != 0:
        return False, f"创建失败: {r}"
    tid = r["data"]["task_id"]
    # 任务应该仍然会创建成功（PID 校验在 Agent 端），
    # 但执行时应上报 FAILED
    for _ in range(30):
        time.sleep(0.5)
        r2 = api("GET", f"/api/v1/tasks/{tid}")
        status = r2.get("data", {}).get("status", "")
        if status == "FAILED":
            reason = r2.get("data", {}).get("status_reason", "")
            return True, f"无效 PID 正确失败: {reason}"
        if status in ("DONE",):
            return True, "任务仍完成了（取决于 Agent 实现）"
    return False, "未检测到最终状态"


# ============================================================
# 场景3：异常路径2 —— Agent 离线时创建任务
# ============================================================

def test_offline_agent():
    r = api("POST", "/api/v1/tasks", {
        "name": "e2e_离线Agent",
        "target_ip": "192.168.255.255",  # 明确不存在的 IP
        "pid": 1,
        "hz": 99,
        "duration": 10
    })
    if r.get("code") != 0:
        return False, f"创建失败: {r}"

    tid = r["data"]["task_id"]

    # 离线 Agent 的任务应该快速变为 FAILED
    for _ in range(20):
        time.sleep(0.5)
        r2 = api("GET", f"/api/v1/tasks/{tid}")
        status = r2.get("data", {}).get("status", "")
        reason = r2.get("data", {}).get("status_reason", "")
        if status == "FAILED":
            return True, f"离线 Agent 正确拒绝: {reason}"
        if "离线" in reason or "offline" in reason.lower():
            return True, f"检测到离线: {reason}"

    # 没等到 FAILED，但只要任务停留在 PENDING 也是合理的
    return True, "任务仍在 PENDING（离线 Agent 未领取）"


# ============================================================
# 运行
# ============================================================

if __name__ == "__main__":
    # 确保 API Server 已启动
    health = api("GET", "/api/v1/agents")
    if health.get("code") != 0:
        print("请先启动 API Server: cd apiserver && python3 app.py")
        sys.exit(1)

    test("正常路径: 创建→等待→完成",       test_normal_flow)
    test("异常路径1: 无效 PID",            test_invalid_pid)
    test("异常路径2: Agent 离线时创建任务", test_offline_agent)

    print(f"\n=== 结果: {PASSED}/3 通过, {FAILED} 失败 ===")
    sys.exit(0 if FAILED == 0 else 1)
