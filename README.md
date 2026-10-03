# QtLANChat

基于 **Qt 6 / C++17** 的局域网聊天客户端，Widgets 界面 + Network 模块，CMake 构建。

项目按「天」迭代推进，当前进度：**Day4（多线程网络层）**。

## 特性

- 地址 / 端口 / 用户名 的顶部工具栏，Connect / Disconnect 控制
- 聊天区 + 在线用户列表（可拖动分隔条）、日志区
- 网络对象跑在**独立工作线程**：`NetworkWorker` 持有 `QTcpSocket` + `ReceiveBuffer`，UI 线程只负责绘制
- `ClientService` 作为线程门面：内部 `moveToThread` + `QThread`，对外信号与 Day2 完全一致，UI 层无感知
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

## 线程模型

网络层全部搬进一个专用工作线程，主线程只剩界面：

| 对象 | 活在哪 | 干什么 |
| --- | --- | --- |
| `MainWindow` | UI 线程 | 只画界面、只连信号 |
| `ClientService` | UI 线程 | 持有 `QThread`；把 UI 的调用排队投递到工作线程，把 worker 的信号原样转发回来 |
| `NetworkWorker` | **工作线程** | 持有 `QTcpSocket` + `ReceiveBuffer`，收发、分帧、解包 |

三条纪律：

1. **线程归属由 `moveToThread` 决定，不是由谁 `new` 决定**。socket 也放在 worker 的
   `start()`（由 `QThread::started` 触发）里创建，保证父子同线程。
2. **跨线程连接默认就是排队投递**（AutoConnection → QueuedConnection）。UI 想指挥 worker，
   走 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`；直接写 `m_worker->sendPacket(...)`
   是普通函数调用，**仍在 UI 线程执行**，等于没跨线程。
3. **排队参数必须可拷贝**，自定义类型要先注册：`qRegisterMetaType<proto::Packet>("proto::Packet")`，
   否则运行期报 `Cannot queue arguments of type 'proto::Packet'`，槽根本不触发。

收尾顺序：`stop`（`BlockingQueuedConnection`，让 worker 在自己线程里 `abort`）→ `quit()` →
`wait(3000)`；全程不使用 `terminate()`。断开时清空接收缓冲，避免残留半包接到下一条连接上。

本机实测：服务端持续 **5000 条/秒 × 30 秒**（≈15 万包）**0 丢包**，期间 UI 事件循环
最大迟到 **17 ms**；把 `sleep(2)` 故意塞进 worker 后，包晚到 2001 ms 而 UI 依旧顺滑。
（把 2000 条消息在一瞬间灌进聊天框会有 ~0.85 s 卡顿——那是 UI 逐条渲染的代价，
不是网络线程拖住了界面，批处理/节流留待后续优化。）

## Day5：聊天 + 心跳 + 断线重连

网络线程里现在住着三个对象（都在 `ClientService` 构造时 `moveToThread`）：

- `NetworkWorker` —— `QTcpSocket` + 收包拆帧（Day4）
- `ConnectionManager` —— 心跳保活（5 s 一发，ACK 超时 15 s，1 s 轮询检查）+ 断线重连
- `ChatManager` —— 只做消息语义（登录 / 私聊 / 群聊 / 用户列表），**不碰 socket**，
  出方向只 `emit requestPacket(proto::Packet)`，入方向只 `onPacketReceived(...)`

状态收敛到 `ConnectionState { Disconnected, Connecting, Connected, Reconnecting }`（枚举 +
`Q_DECLARE_METATYPE` + `qRegisterMetaType`），UI 的状态只能来自 `ConnectionManager::stateChanged`
这**一个**数据源。重连退避 1→2→4→8→16→32 s 封顶 30 s，连上即清零；**主动断开不触发重连**，
靠 `NetworkWorker::transportLost`（仅非主动断开/连接失败才发）区分。重连成功后由
`ChatManager::onServerConnected()` 用记住的名字自动重新 `LOGIN`；登录被拒则清空名字，
避免重连后拿一个已被占用的名字抢登。

> 本地验证：`protocol_selftest` 16 项回归全过；`day5_probe`（自己拉起 `chat_server`，两个真实客户端）
> 覆盖登录 / 用户列表 / 私聊 / 群聊 / 重名拒绝 / 特殊字符往返 / 心跳保活 / 杀服务器重连 / 重连重登 /
> 优雅退出，**17 项全部 PASS**。

## 目录结构

```
QtLANChat/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── ui/            # MainWindow：界面与信号槽接线（Day5 加入输入框 + 发送）
│   ├── core/          # ClientService（线程门面）+ ConnectionManager（心跳/重连）+ ChatManager（消息语义）
│   ├── network/       # NetworkWorker：工作线程里的 QTcpSocket + 分包接入；ConnectionState 枚举
│   └── protocol/      # 帧格式、序列化、ReceiveBuffer（粘包/拆包）
└── tools/
    ├── echo_server/        # 本地联调用的回显服务器（Day2）
    ├── chat_server/        # Day5 联调服务器：登录/用户列表/私聊/群聊/心跳
    ├── day5_probe/         # Day5 验收探针（自动拉起 chat_server 跑 A~E）
    ├── gui_probe/          # Day5 GUI 取证（窗口截图）
    └── protocol_selftest.cpp  # 协议层离线自测（不依赖网络）
```

## 路线图

- [x] **Day1** — Qt6 / CMake 骨架，UI 布局
- [x] **Day2** — 真实 TCP 连接（连 / 断）
- [x] **Day3** — 自定义分帧协议（16 字节定长头 + payload），替代临时 `'\n'` 分帧
- [x] **Day4** — 多线程网络层：网络对象搬进工作线程（`moveToThread` + 跨线程信号槽）
- [x] **Day5** — 聊天（登录/私聊/群聊/用户列表）+ 心跳保活 + 断线自动重连（状态机 + 指数退避）
- [ ] **Day6** — 文件传输 / 更多 UI 细节
- [ ] **Day7** — 打包发布

## License

暂未指定。
