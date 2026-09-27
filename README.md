# QtLANChat

基于 **Qt 6 / C++17** 的局域网聊天客户端，Widgets 界面 + Network 模块，CMake 构建。

项目按「天」迭代推进，当前进度：**Day3（自定义分帧协议）**。

## 特性

- 地址 / 端口 / 用户名 的顶部工具栏，Connect / Disconnect 控制
- 聊天区 + 在线用户列表（可拖动分隔条）、日志区
- 网络层 `TcpClient` 独立封装：异步连接，全部走 Qt 信号槽，不阻塞 UI 线程
- 自定义分帧协议 `protocol/`：16 字节定长头（全部 Big Endian）+ payload，`ReceiveBuffer` 负责粘包 / 拆包
- 协议层收到完整包才回调（`packetReceived`），JSON 只在整包边界解析
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

## 协议层自测

```powershell
build/bin/protocol_selftest.exe    # 同样需要把 Qt bin 加入 PATH
```

不依赖网络的离线用例：完整包 / 半包 / 三分包 / 粘连 / 空载荷 / 坏 magic /
超长声明 / 一字节一字节喂 / 文件裸二进制往返。退出码 = 失败用例数。

## 协议

所有消息都走同一个帧格式：**16 字节定长头 + 变长 payload**，头部字段一律 Big Endian，
中文等文本用 UTF-8 编码的紧凑 JSON（`QJsonDocument::Compact`，不用 `Indented`）。

| 字段 | 字节 | 说明 |
| --- | --- | --- |
| magic | 2 | 固定 `0x4C43`（线上字节 `4C 43`） |
| version | 1 | 协议版本，当前 `1` |
| type | 1 | 消息类型（登录 / 聊天 / 用户列表 / 文件 / 心跳 / 错误 …） |
| flags | 2 | 保留，当前全 0 |
| requestId | 4 | 请求序号，用于请求 / 响应配对 |
| payloadLen | 4 | payload 字节数（不含头），上限 8 MiB |
| reserved | 2 | 补齐到 16 字节，当前全 0 |

接收侧 `ReceiveBuffer` 的语义：头部不够或包未收全时**一个字节都不消费**，等下一次 `readyRead`；
只有凑齐一个完整包才取出来。`magic` 非法或 `payloadLen` 超过上限则上报错误并断开连接，
不会为超长声明分配内存。

## 目录结构

```
QtLANChat/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── ui/            # MainWindow：界面与信号槽接线
│   ├── network/       # TcpClient：QTcpSocket 封装 + 分包接入
│   └── protocol/      # 帧格式、序列化、ReceiveBuffer（粘包/拆包）
└── tools/
    ├── echo_server/        # 本地联调用的回显服务器
    └── protocol_selftest.cpp  # 协议层离线自测（不依赖网络）
```

## 路线图

- [x] **Day1** — Qt6 / CMake 骨架，UI 布局
- [x] **Day2** — 真实 TCP 连接（连 / 断）
- [x] **Day3** — 自定义分帧协议（16 字节定长头 + payload），替代临时 `'\n'` 分帧
- [ ] **Day4** — 消息协议与用户列表同步
- [ ] **Day5** — 连接状态枚举 + 自动重连（指数退避）
- [ ] **Day6** — 文件传输 / 更多 UI 细节
- [ ] **Day7** — 打包发布

## License

暂未指定。
