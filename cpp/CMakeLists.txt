cmake_minimum_required(VERSION 3.16)
project(mini-drop VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# TODO Day1: 安装依赖后，取消下面三行的注释
# ============================================================
# find_package(Protobuf REQUIRED)
# find_package(gRPC CONFIG REQUIRED)
# find_package(spdlog REQUIRED)

# ============================================================
# TODO Day2: proto 编译函数（你先手写 grpc 命令理解原理）
# ============================================================
# add_proto_library(drop_proto ${PROTO_FILES})

# ============================================================
# TODO Day5-6: 取消注释，逐步添加你写的 .cpp 文件
# ============================================================
# add_library(drop_common STATIC
#   common/Log.h
#   common/ProcStats.cpp
#   common/PerfProfiler.cpp
# )
#
# add_executable(drop_server
#   server/main.cpp
#   server/TaskQueue.h
#   server/HealthCheckService.cpp
#   server/ControlService.cpp
# )
#
# add_executable(drop_agent
#   agent/main.cpp
#   agent/HeartbeatChannel.cpp
#   agent/HotmethodChannel.cpp
# )
