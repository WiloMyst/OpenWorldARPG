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

pkg_check_modules(HIREDIS REQUIRED hiredis)

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

# 控制面 (GameServer -> VHServer 服务间): 对话流吊销
add_custom_command(
    OUTPUT "${GENERATED_DIR}/admin.pb.cc"
           "${GENERATED_DIR}/admin.pb.h"
           "${GENERATED_DIR}/admin.grpc.pb.cc"
           "${GENERATED_DIR}/admin.grpc.pb.h"
    COMMAND ${PROTOC_EXECUTABLE}
    ARGS --cpp_out="${GENERATED_DIR}"
         --grpc_out="${GENERATED_DIR}"
         --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
         -I "${PROTO_DIR}"
         "${PROTO_DIR}/admin.proto"
    DEPENDS "${PROTO_DIR}/admin.proto"
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
    src/net/grpc_server.cpp
    src/net/player_session.cpp
    src/net/unary_call.cpp
    src/net/dialogue_revoker.cpp
    src/session/session_manager.cpp
    src/session/session_handler.cpp
    src/logic/game_logic.cpp
    src/logic/item_database.cpp
    src/logic/inventory_manager.cpp
    src/logic/initial_archive_loader.cpp
    src/storage/mysql_store.cpp
    src/storage/redis_store.cpp
    src/world/aoi_manager.cpp
    src/world/world_manager.cpp
    src/combat/combat_manager.cpp
    "${GENERATED_DIR}/game.pb.cc"
    "${GENERATED_DIR}/game.grpc.pb.cc"
    "${GENERATED_DIR}/admin.pb.cc"
    "${GENERATED_DIR}/admin.grpc.pb.cc"
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

# 7. Phase 1 状态同步 + AOI 集成测试
add_executable(test_aoi
    tests/test_aoi.cpp
    "${GENERATED_DIR}/game.pb.cc"
    "${GENERATED_DIR}/game.grpc.pb.cc"
)
target_include_directories(test_aoi PRIVATE ${GAME_INCLUDES})
target_link_libraries(test_aoi PRIVATE ${GAME_LIBS})

# 8. 测试数据清理工具 (替代 mysql CLI: WSL 无 root 装不了 mysql-client)
add_executable(cleanup_test_data
    tools/cleanup_test_data.cpp
)
target_include_directories(cleanup_test_data PRIVATE ${MYSQL_INCLUDE_DIRS})
target_link_libraries(cleanup_test_data PRIVATE ${MYSQL_LIBRARIES})

# 9. Phase 2 自定义 TCP 协议集成测试 已随 TCP 移除
# 10. Phase 3 服务器权威战斗单测 (配置加载/刷怪/伤害结算/击杀判定)
# 战斗管理器广播依赖 SessionManager, 故一并链接会话/世界实现 (测试不注入会话, 广播走空分支)
add_executable(test_combat
    tests/test_combat.cpp
    src/combat/combat_manager.cpp
    src/session/session_manager.cpp
    src/world/world_manager.cpp
    src/world/aoi_manager.cpp
    "${GENERATED_DIR}/game.pb.cc"
)
target_include_directories(test_combat PRIVATE ${GAME_INCLUDES})
target_link_libraries(test_combat PRIVATE
    ${GRPC_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
    spdlog::spdlog
    yaml-cpp
    nlohmann_json::nlohmann_json
    pthread
)
