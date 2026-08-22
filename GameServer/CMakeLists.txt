cmake_minimum_required(VERSION 3.15)
project(GameServer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. 依赖查找
find_program(PROTOC_EXECUTABLE NAMES protoc)
find_program(GRPC_CPP_PLUGIN_EXECUTABLE NAMES grpc_cpp_plugin)

if(NOT PROTOC_EXECUTABLE OR NOT GRPC_CPP_PLUGIN_EXECUTABLE)
    message(FATAL_ERROR "protoc or grpc_cpp_plugin not found")
endif()

find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(nlohmann_json REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++)
pkg_check_modules(PROTOBUF REQUIRED protobuf)
pkg_check_modules(MYSQL REQUIRED mysqlclient)

# hiredis: 优先系统包; 无 root 环境回退到本地解包目录 (~/hiredis-local/root,
# 由 scripts/setup_hiredis_local.sh 生成: apt-get download + dpkg -x)
pkg_check_modules(HIREDIS hiredis)
if(NOT HIREDIS_FOUND)
    set(HIREDIS_ROOT "$ENV{HOME}/hiredis-local/root")
    find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h
              HINTS "${HIREDIS_ROOT}/usr/include" REQUIRED)
    find_library(HIREDIS_LIBRARY NAMES hiredis
                 HINTS "${HIREDIS_ROOT}/usr/lib/x86_64-linux-gnu" REQUIRED)
    set(HIREDIS_INCLUDE_DIRS "${HIREDIS_INCLUDE_DIR}")
    set(HIREDIS_LIBRARIES "${HIREDIS_LIBRARY}")
endif()

# 2. Protobuf 代码生成
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/protos")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
    OUTPUT "${GENERATED_DIR}/game.pb.cc"
           "${GENERATED_DIR}/game.pb.h"
           "${GENERATED_DIR}/game.grpc.pb.cc"
           "${GENERATED_DIR}/game.grpc.pb.h"
    COMMAND ${PROTOC_EXECUTABLE}
    ARGS --cpp_out="${GENERATED_DIR}"
         --grpc_out="${GENERATED_DIR}"
         --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
         -I "${PROTO_DIR}"
         "${PROTO_DIR}/game.proto"
    DEPENDS "${PROTO_DIR}/game.proto"
)

# 3. 公共编译配置
set(GAME_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${GENERATED_DIR}"
    ${GRPC_INCLUDE_DIRS}
    ${PROTOBUF_INCLUDE_DIRS}
    ${MYSQL_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIRS}
)

set(GAME_LIBS
    ${GRPC_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
    ${MYSQL_LIBRARIES}
    ${HIREDIS_LIBRARIES}
    spdlog::spdlog
    yaml-cpp
    nlohmann_json::nlohmann_json
    pthread
)

# 4. 服务器
add_executable(game_server
    src/main.cpp
    src/core/grpc_server.cpp
    src/core/player_session.cpp
    src/session/session_manager.cpp
    src/logic/game_logic.cpp
    src/logic/item_database.cpp
    src/logic/inventory_manager.cpp
    src/storage/mysql_store.cpp
    src/storage/redis_store.cpp
    "${GENERATED_DIR}/game.pb.cc"
    "${GENERATED_DIR}/game.grpc.pb.cc"
)
target_include_directories(game_server PRIVATE ${GAME_INCLUDES})
target_link_libraries(game_server PRIVATE ${GAME_LIBS})

# 5. 集成测试客户端
add_executable(test_client
    tests/test_client.cpp
    "${GENERATED_DIR}/game.pb.cc"
    "${GENERATED_DIR}/game.grpc.pb.cc"
)
target_include_directories(test_client PRIVATE ${GAME_INCLUDES})
target_link_libraries(test_client PRIVATE ${GAME_LIBS})

# 6. 跨服票据探针 (配合 VHServer/tests/test_cross_server.py 验证信令面->数据面闭环)
add_executable(token_probe
    tests/token_probe.cpp
    "${GENERATED_DIR}/game.pb.cc"
    "${GENERATED_DIR}/game.grpc.pb.cc"
)
target_include_directories(token_probe PRIVATE ${GAME_INCLUDES})
target_link_libraries(token_probe PRIVATE ${GAME_LIBS})
