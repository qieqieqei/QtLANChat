cmake_minimum_required(VERSION 3.21)

project(QtLANChat VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 必须在 add_executable 之前设置，否则不生效
set(CMAKE_AUTOMOC ON)   # 处理 Q_OBJECT，生成 moc
set(CMAKE_AUTORCC ON)   # 处理 .qrc 资源（Day1 可留空）
set(CMAKE_AUTOUIC ON)   # 处理 .ui 文件（Day1 可留空）

# Windows 上把 exe 输出到统一 bin 目录，方便双击运行
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# Day3 协议层：header / packet / receive buffer / codec（不含 UI 依赖，方便 Day4 丢进工作线程）
set(PROTOCOL_HEADERS
    src/protocol/MessageType.h
    src/protocol/PacketHeader.h
    src/protocol/Packet.h
    src/protocol/ReceiveBuffer.h
    src/protocol/ProtocolCodec.h
)
set(PROTOCOL_SOURCES
    src/protocol/PacketHeader.cpp
    src/protocol/ReceiveBuffer.cpp
    src/protocol/ProtocolCodec.cpp
)

# Day4/Day5 线程层：NetworkWorker（干活，住网络线程）+ 连接状态枚举
set(NETWORK_HEADERS
    src/network/ConnectionState.h
    src/network/NetworkWorker.h
)
set(NETWORK_SOURCES
    src/network/NetworkWorker.cpp
)
# Day4/Day5 核心层：ClientService（门面，留 UI 线程）+ ConnectionManager（心跳/重连）+ ChatManager（消息语义）
set(CORE_HEADERS
    src/core/ClientService.h
    src/core/ConnectionManager.h
    src/core/ChatManager.h
)
set(CORE_SOURCES
    src/core/ClientService.cpp
    src/core/ConnectionManager.cpp
    src/core/ChatManager.cpp
)

add_executable(QtLANChat
    src/main.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    ${NETWORK_HEADERS}               # Day4：NetworkWorker
    ${NETWORK_SOURCES}               # Day4：NetworkWorker
    ${CORE_HEADERS}                  # Day4：ClientService
    ${CORE_SOURCES}                  # Day4：ClientService
    ${PROTOCOL_HEADERS}              # Day3 新增
    ${PROTOCOL_SOURCES}              # Day3 新增
)

target_include_directories(QtLANChat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(QtLANChat PRIVATE
    Qt6::Widgets
    Qt6::Network      # Day2 才真正用到，先链上没关系
)

# 只作用于本 target，不要用 add_compile_options 全局污染
# （Qt6 的 QThread 在 Qt6::Core 里，不需要额外 find_package）
if (MSVC)
    target_compile_options(QtLANChat PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(QtLANChat PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(QtLANChat PROPERTIES WIN32_EXECUTABLE ON)  # Windows 不弹黑框

# ---------------------------------------------------------------------------
# Day2 工具：迷你 echo 服务器，只用来验证客户端「连没连上、数据通不通」
# 它不是交付物，Day7 打包时不要带上（-DQTLANCHAT_BUILD_TOOLS=OFF 可整体关掉）
# ---------------------------------------------------------------------------
option(QTLANCHAT_BUILD_TOOLS "Build helper tools (echo_server) for local verification" ON)

if(QTLANCHAT_BUILD_TOOLS)
    add_executable(echo_server tools/echo_server/main.cpp)
    target_link_libraries(echo_server PRIVATE Qt6::Network)

    # Day5 联调服务器：登录 / 用户列表 / 私聊 / 群聊 / 心跳（复用协议层，不依赖 UI）
    add_executable(chat_server tools/chat_server/main.cpp ${PROTOCOL_HEADERS} ${PROTOCOL_SOURCES})
    target_include_directories(chat_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(chat_server PRIVATE Qt6::Network)

    # Day5 验收探针：自动拉起 chat_server，两个客户端跑完 A/B/C/D/E 验收项（不是交付物）
    add_executable(day5_probe tools/day5_probe/main.cpp
        ${PROTOCOL_HEADERS} ${PROTOCOL_SOURCES}
        ${NETWORK_HEADERS}  ${NETWORK_SOURCES}
        ${CORE_HEADERS}     ${CORE_SOURCES})
    target_include_directories(day5_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(day5_probe PRIVATE Qt6::Network)

    # Day5 GUI 取证：真实 windows 平台跑 UI 并截图（不是交付物）
    add_executable(gui_probe tools/gui_probe/main.cpp
        src/ui/MainWindow.h        src/ui/MainWindow.cpp
        ${PROTOCOL_HEADERS} ${PROTOCOL_SOURCES}
        ${NETWORK_HEADERS}  ${NETWORK_SOURCES}
        ${CORE_HEADERS}     ${CORE_SOURCES})
    target_include_directories(gui_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(gui_probe PRIVATE Qt6::Widgets Qt6::Network)

    # Day3 一次性自测：粘包/拆包 8 个测试向量（不是交付物）
    add_executable(protocol_selftest tools/protocol_selftest.cpp ${PROTOCOL_HEADERS} ${PROTOCOL_SOURCES})
    target_include_directories(protocol_selftest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(protocol_selftest PRIVATE Qt6::Core)

    if(MSVC)
        target_compile_options(echo_server PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(chat_server PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(day5_probe PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(gui_probe PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(protocol_selftest PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(echo_server PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(chat_server PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(day5_probe PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(gui_probe PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(protocol_selftest PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
