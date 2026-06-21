#!/usr/bin/env python3
# ============================================================
# Mini-Drop API Server (Flask)
#
# HTTP → gRPC 桥接 + SQLite 任务/Agent/审计日志存储。
# 对应 PLAN.md 5.1，5 个路由 + 3 张表。
# ============================================================

import os
import time
import sqlite3
import uuid
from datetime import datetime
from flask import Flask, request, jsonify, g

app = Flask(__name__)

DATABASE = os.path.join(os.path.dirname(__file__), "mini_drop.db")

# ============================================================
# 数据库工具
# ============================================================

def get_db():
    if "db" not in g:
        g.db = sqlite3.connect(DATABASE)
        g.db.row_factory = sqlite3.Row
        g.db.execute("PRAGMA journal_mode=WAL")
    return g.db

@app.teardown_appcontext
def close_db(exception):
    db = g.pop("db", None)
    if db is not None:
        db.close()

def init_db():
    db = sqlite3.connect(DATABASE)
    db.execute("""
        CREATE TABLE IF NOT EXISTS tasks (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            task_id   TEXT UNIQUE NOT NULL,
            name      TEXT DEFAULT '',
            target_ip TEXT DEFAULT '',
            pid       INTEGER DEFAULT 0,
            hz        INTEGER DEFAULT 99,
            duration  INTEGER DEFAULT 10,
            status    TEXT DEFAULT 'PENDING',
            status_reason TEXT DEFAULT '',
            output_file  TEXT DEFAULT '',
            created_at   TEXT DEFAULT (datetime('now','localtime')),
            updated_at   TEXT DEFAULT (datetime('now','localtime'))
        )
    """)
    db.execute("""
        CREATE TABLE IF NOT EXISTS agents (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            hostname  TEXT DEFAULT '',
            ip_addr   TEXT UNIQUE NOT NULL,
            uid       TEXT DEFAULT '',
            version   TEXT DEFAULT '',
            is_online INTEGER DEFAULT 1,
            cpu_percent REAL DEFAULT 0,
            rss_kb      INTEGER DEFAULT 0,
            last_heartbeat TEXT DEFAULT (datetime('now','localtime'))
        )
    """)
    db.execute("""
        CREATE TABLE IF NOT EXISTS audit_log (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            task_id   TEXT DEFAULT '',
            event     TEXT DEFAULT '',
            detail    TEXT DEFAULT '',
            created_at TEXT DEFAULT (datetime('now','localtime'))
        )
    """)
    db.commit()
    db.close()

# ============================================================
# 审计日志
# ============================================================

def write_audit(task_id, event, detail=""):
    db = get_db()
    db.execute(
        "INSERT INTO audit_log (task_id, event, detail) VALUES (?, ?, ?)",
        (task_id, event, detail)
    )
    db.commit()

# ============================================================
# 状态迁移辅助
# ============================================================

def change_task_status(task_id, new_status, reason=""):
    db = get_db()
    db.execute(
        "UPDATE tasks SET status=?, status_reason=?, updated_at=datetime('now','localtime') WHERE task_id=?",
        (new_status, reason, task_id)
    )
    db.commit()
    write_audit(task_id, new_status, reason)

# ============================================================
# 路由 1：创建采集任务  POST /api/v1/tasks
# ============================================================

@app.route("/api/v1/tasks", methods=["POST"])
def create_task():
    data = request.get_json() or {}
    tid  = "task-" + uuid.uuid4().hex[:8]
    db   = get_db()

    db.execute(
        """INSERT INTO tasks (task_id, name, target_ip, pid, hz, duration, status)
           VALUES (?, ?, ?, ?, ?, ?, 'PENDING')""",
        (tid,
         data.get("name", "CPU采样"),
         data.get("target_ip", "127.0.0.1"),
         data.get("pid", 1),
         data.get("hz", 99),
         data.get("duration", 10))
    )
    db.commit()
    write_audit(tid, "PENDING", f"创建任务 {data.get('name', 'CPU采样')}")

    # TODO Day5 联调：调 gRPC ControlService.CreateTask()
    # 暂时直接模拟状态流转（等联调时替换）
    change_task_status(tid, "RUNNING", "Agent 已领取")
    print(f"[API] 创建任务 {tid} → {data.get('target_ip', '127.0.0.1')}")

    return jsonify({"code": 0, "data": {"task_id": tid}}), 200

# ============================================================
# 路由 2：任务列表  GET /api/v1/tasks
# ============================================================

@app.route("/api/v1/tasks", methods=["GET"])
def list_tasks():
    db   = get_db()
    rows = db.execute(
        "SELECT * FROM tasks ORDER BY created_at DESC LIMIT 50"
    ).fetchall()
    return jsonify({
        "code": 0,
        "data": [dict(r) for r in rows]
    }), 200

# ============================================================
# 路由 3：单个任务详情  GET /api/v1/tasks/<task_id>
# ============================================================

@app.route("/api/v1/tasks/<task_id>", methods=["GET"])
def get_task(task_id):
    db  = get_db()
    row = db.execute("SELECT * FROM tasks WHERE task_id=?", (task_id,)).fetchone()
    if not row:
        return jsonify({"code": 404, "msg": "任务不存在"}), 404
    return jsonify({"code": 0, "data": dict(row)}), 200

# ============================================================
# 路由 4：Agent 列表  GET /api/v1/agents
# ============================================================

@app.route("/api/v1/agents", methods=["GET"])
def list_agents():
    db   = get_db()
    rows = db.execute("SELECT * FROM agents ORDER BY last_heartbeat DESC").fetchall()
    return jsonify({
        "code": 0,
        "data": [dict(r) for r in rows]
    }), 200

# ============================================================
# 路由 5：Agent 心跳  POST /api/v1/agent/heartbeat
# ============================================================

@app.route("/api/v1/agent/heartbeat", methods=["POST"])
def agent_heartbeat():
    data = request.get_json() or {}
    ip   = data.get("ip_addr", "127.0.0.1")
    db   = get_db()

    existing = db.execute("SELECT id FROM agents WHERE ip_addr=?", (ip,)).fetchone()
    if existing:
        db.execute(
            """UPDATE agents SET hostname=?, uid=?, version=?,
               is_online=1, cpu_percent=?, rss_kb=?,
               last_heartbeat=datetime('now','localtime')
               WHERE ip_addr=?""",
            (data.get("hostname", ""),
             data.get("uid", ""),
             data.get("version", ""),
             data.get("cpu_percent", 0),
             data.get("rss_kb", 0),
             ip)
        )
    else:
        db.execute(
            """INSERT INTO agents (hostname, ip_addr, uid, version, is_online, cpu_percent, rss_kb)
               VALUES (?, ?, ?, ?, 1, ?, ?)""",
            (data.get("hostname", ""),
             ip,
             data.get("uid", ""),
             data.get("version", ""),
             data.get("cpu_percent", 0),
             data.get("rss_kb", 0))
        )
    db.commit()
    return jsonify({"code": 0, "msg": "ok"}), 200

# ============================================================
# 路由 6：审计日志  GET /api/v1/audit
# ============================================================

@app.route("/api/v1/audit", methods=["GET"])
def list_audit():
    db   = get_db()
    rows = db.execute(
        "SELECT * FROM audit_log ORDER BY created_at DESC LIMIT 100"
    ).fetchall()
    return jsonify({"code": 0, "data": [dict(r) for r in rows]}), 200

# ============================================================
# 入口
# ============================================================

if __name__ == "__main__":
    init_db()
    print("API Server 启动: http://0.0.0.0:5000")
    app.run(host="0.0.0.0", port=5000, debug=True)
