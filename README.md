# QtLANChat

基于 **Qt 6 / C++17** 的局域网聊天客户端，Widgets 界面 + Network 模块，CMake 构建。

项目按「天」迭代推进，当前进度：**Day2（真实 TCP 连接）**。

## 特性

- 地址 / 端口 / 用户名 的顶部工具栏，Connect / Disconnect 控制
- 聊天区 + 在线用户列表（可拖动分隔条）、日志区
- 网络层 `TcpClient` 独立封装：异步连接，全部走 Qt 信号槽，不阻塞 UI 线程
- 状态去重上报（未连接 / 连接中 / 已连接），断开时清理接收缓冲
- 中文按 UTF-8 收发，避免 MSVC 下 `toLocal8Bit()` 的 GBK 乱码
- 附带调试用迷你 `echo_server`

## 环境要求

- Qt **6.11.2**（kit：`msvc2022_64`，需要 `Widgets` + `Network`）
- CMake ≥ 3.21
- Ninja（或其它 CMake 生成器）
- MSVC（Visual Studio 2022，C++17）

## 构建

在 **Developer PowerShell / VS 开发者命令行** 中执行（示例路径请替换为你自己的 Qt 安装目录）：

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_PREFIX_PATH="<Qt>/6.11.2/msvc2022_64" `
  -DCMAKE_MAKE_PROGRAM="<Qt>/Tools/Ninja/ninja.exe"

cmake --build build
```

产物：`build/bin/QtLANChat.exe`

直接运行 exe 需要把 `<Qt>/6.11.2/msvc2022_64/bin` 加入 `PATH`（否则缺 `Qt6Widgets.dll`）。

## 运行

1. 启动一个 echo 服务端用于联调：
   ```powershell
   build/bin/echo_server.exe 8888
   ```
2. 启动客户端 `build/bin/QtLANChat.exe`，填地址 `127.0.0.1`、端口 `8888`、任意用户名，点 **Connect**。
3. 连接成功后日志区显示「已连接」。

> `echo_server` 只是本地验证工具，不属于客户端交付物，可用
> `-DQTLANCHAT_BUILD_TOOLS=OFF` 整体关闭。

## 目录结构

```
QtLANChat/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── ui/            # MainWindow：界面与信号槽接线
│   └── network/       # TcpClient：QTcpSocket 封装
└── tools/
    └── echo_server/   # 本地联调用的回显服务器
```

## 路线图

- [x] **Day1** — Qt6 / CMake 骨架，UI 布局
- [x] **Day2** — 真实 TCP 连接（连 / 断 / 收发一行文本）
- [ ] **Day3** — 自定义分帧协议（16 字节头 + payload），替代临时 `'\n'` 分帧
- [ ] **Day4** — 消息协议与用户列表同步
- [ ] **Day5** — 连接状态枚举 + 自动重连（指数退避）
- [ ] **Day6** — 文件传输 / 更多 UI 细节
- [ ] **Day7** — 打包发布

## License

暂未指定。
