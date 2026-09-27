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

add_executable(QtLANChat
    src/main.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/network/TcpClient.h          # Day2 新增
    src/network/TcpClient.cpp        # Day2 新增
)

target_include_directories(QtLANChat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(QtLANChat PRIVATE
    Qt6::Widgets
    Qt6::Network      # Day2 才真正用到，先链上没关系
)

# 只作用于本 target，不要用 add_compile_options 全局污染
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

    if(MSVC)
        target_compile_options(echo_server PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(echo_server PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
