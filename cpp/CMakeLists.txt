cmake_minimum_required(VERSION 3.16)
project(mini-drop VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# 依赖查找
# ============================================================
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(nlohmann_json REQUIRED)

# ============================================================
# Proto 文件编译
# ============================================================
set(PROTO_DIR "${CMAKE_SOURCE_DIR}/../proto")

set(PROTO_FILES
    ${PROTO_DIR}/common.proto
    ${PROTO_DIR}/healthcheck.proto
    ${PROTO_DIR}/hotmethod.proto
    ${PROTO_DIR}/control.proto
    ${PROTO_DIR}/init.proto
)

# 编译 proto → .pb.cc + .grpc.pb.cc
foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    set(PB_CC  "${CMAKE_CURRENT_BINARY_DIR}/proto/${PROTO_NAME}.pb.cc")
    set(GRPC_CC "${CMAKE_CURRENT_BINARY_DIR}/proto/${PROTO_NAME}.grpc.pb.cc")

    add_custom_command(
        OUTPUT ${PB_CC} ${GRPC_CC}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            -I ${PROTO_DIR}
            --cpp_out=${CMAKE_CURRENT_BINARY_DIR}/proto
            --grpc_out=${CMAKE_CURRENT_BINARY_DIR}/proto
            --plugin=protoc-gen-grpc=${gRPC_CPP_PLUGIN}
            ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Compiling proto: ${PROTO_NAME}"
    )
    list(APPEND PROTO_SRCS ${PB_CC} ${GRPC_CC})
endforeach()

# ============================================================
# 头文件检索路径
# ============================================================
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/proto
    ${CMAKE_CURRENT_SOURCE_DIR}/../
)

# ============================================================
# 公共库 —— ProcStats + PerfProfiler
# ============================================================
add_library(drop_common STATIC
    common/ProcStats.cpp
    common/PerfProfiler.cpp
)
target_link_libraries(drop_common
    gRPC::grpc++
    ${Protobuf_LIBRARIES}
)

# ============================================================
# drop_server 二进制
# ============================================================
add_executable(drop_server
    server/main.cpp
    server/HealthCheckService.cpp
    server/ControlService.cpp
    server/HotmethodService.cpp
    ${PROTO_SRCS}
)
target_link_libraries(drop_server
    drop_common
    gRPC::grpc++
    ${Protobuf_LIBRARIES}
)

# ============================================================
# drop_agent 二进制
# ============================================================
add_executable(drop_agent
    agent/main.cpp
    agent/Config.cpp
    agent/HeartbeatChannel.cpp
    agent/HotmethodChannel.cpp
    ${PROTO_SRCS}
)
target_link_libraries(drop_agent
    drop_common
    gRPC::grpc++
    ${Protobuf_LIBRARIES}
    nlohmann_json::nlohmann_json
)
