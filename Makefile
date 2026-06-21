# ============================================================
# Mini-Drop Makefile
#
# 常用命令:
#   make build    — 编译 C++ 二进制
#   make test     — 运行 C++ 单元测试
#   make demo     — docker compose up 一键启动
#   make clean    — 清理编译产物
# ============================================================

.PHONY: build test clean demo e2e

# ---- C++ 编译 ----
build:
	@mkdir -p cpp/build && cd cpp/build && \
		cmake .. -DCMAKE_BUILD_TYPE=Debug && \
		make -j$$(nproc)
	@echo "二进制: cpp/build/drop_server, cpp/build/drop_agent"

# ---- C++ 单元测试 ----
test:
	cd cpp && g++ -std=c++17 -pthread \
		tests/test_taskqueue.cpp \
		-o build/test_taskqueue && ./build/test_taskqueue
	cd cpp && g++ -std=c++17 -pthread \
		tests/test_procstats.cpp \
		common/ProcStats.cpp \
		-o build/test_procstats && ./build/test_procstats

# ---- Docker 一键演示 ----
demo:
	docker compose up --build -d
	@echo "等待服务启动..."
	@sleep 5
	@echo "浏览器打开 http://localhost:5000"
	@docker compose ps

# ---- 端到端集成测试 ----
e2e:
	cd tests && python3 test_e2e.py

# ---- 停止所有服务 ----
stop:
	docker compose down

# ---- 清理 ----
clean:
	rm -rf cpp/build
	docker compose down -v 2>/dev/null || true
