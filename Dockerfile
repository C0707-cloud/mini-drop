# ============================================================
# Mini-Drop Dockerfile · 多阶段构建
#
# 第一阶段：编译 C++ (drop_server + drop_agent)
# 第二阶段：精简运行镜像（只含二进制 + Python 服务）
# ============================================================

# ---- 构建阶段 ----
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# 安装编译工具链
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    g++ \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY cpp/    ./cpp/
COPY proto/  ./proto/

RUN mkdir build && cd build && \
    cmake ../cpp -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# ---- 运行阶段 ----
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# 安装运行时依赖
RUN apt-get update && apt-get install -y --no-install-recommends \
    libgrpc++1 \
    libprotobuf23 \
    python3 \
    python3-pip \
    python3-flask \
    perl \
    linux-tools-common \
    linux-tools-generic \
    && rm -rf /var/lib/apt/lists/* \
    && pip3 install --no-cache-dir grpcio protobuf

WORKDIR /app

# 从构建阶段复制二进制
COPY --from=builder /build/build/drop_server  /app/drop_server
COPY --from=builder /build/build/drop_agent   /app/drop_agent

# 复制 Python 服务
COPY apiserver/   /app/apiserver/
COPY analyzer/    /app/analyzer/
COPY web_ui/      /app/web_ui/
COPY proto/       /app/proto/
COPY cpp/config.json.example /app/config.json

EXPOSE 5000 50051

# 默认启动 API Server
CMD ["python3", "/app/apiserver/app.py"]
