# ConvAI SDK — 实时语音 AI 对话框架

将 ConvAI SDK 集成到 GoldieOS 桌面应用中，实现**语音录制 → AI 对话 → TTS 播放**的完整实时对话流程。

## 项目概述

| 层 | 技术 |
|---|------|
| 平台 | GoldieOS (Windows 10), WS63 嵌入式 (RISC-V) |
| 语言 | C / C++ |
| 音频编解码 | G.711 A-law (ITU-T G.711) + Opus 1.6.1 |
| 传输协议 | WebSocket + JSON + Base64 |
| 音频参数 | 8kHz / 16kHz, 16-bit, Mono PCM |
| 构建系统 | CMake 3.10+ + MinGW (Win10) / RISC-V GCC (WS63) |

## 目录结构

```
convai-sdk/
├── include/convai/                    # SDK 公共 API 头文件
├── src/                               # SDK 核心库 (convai_sdk)
│   ├── api/                           # 公共 API
│   ├── core/                          # 引擎生命周期 / 事件分发
│   ├── protocol/                      # JSON 协议编解码
│   ├── media/                         # 媒体层
│   ├── transport/                     # WebSocket 传输
│   ├── device_mgr/                    # 设备管理
│   ├── utils/                         # 工具 (Base64, AES, RingBuf, cJSON)
│   └── platform/                      # 平台抽象层 (Linux/Win10/WS63)
├── examples/
│   ├── goldieos/                      # ★ GoldieOS 集成示例 (Win10 + WS63, 通过 CONVAI_PLATFORM 切换)
│   ├── win10_demo/                    # Win10 最小示例
│   └── linux_demo/                    # Linux 最小示例
├── tests/                             # 测试 (mock_server.py 等)
└── docs/                              # 架构文档
```

## 快速开始

### 环境要求

| 平台 | 编译器 | CMake | 其他 |
|------|--------|-------|------|
| Windows 10 | [MSYS2 MinGW-w64](https://www.msys2.org/) GCC 16+ | 3.10+ | `mingw32-make` |
| Linux | GCC / Clang | 3.10+ | `make`, `pthread` |
| WS63 嵌入式 | `riscv32-linux-musl-gcc` | 3.10+ | WS63 交叉工具链 |

### Windows 10 — 构建 GoldieOS 桌面应用

```bash
# 1. 配置 (MinGW 环境)
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCONVAI_PLATFORM=win10

# 2. 编译
mingw32-make goldieos -j$(nproc)

# 3. 运行
./examples/goldieos/goldieos.exe
```

生成的可执行文件: `build/examples/goldieos/goldieos.exe`

### Linux — 构建最小示例

```bash
mkdir build && cd build
cmake .. -DCONVAI_PLATFORM=linux
make linux_demo -j$(nproc)
```

### WS63 嵌入式 — 交叉编译

GoldieOS WS63 已合并到 `examples/goldieos/`，通过 `CONVAI_PLATFORM=ws63` 切换：

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles" \
    -DCONVAI_PLATFORM=ws63 \
    -DCMAKE_C_COMPILER=riscv32-linux-musl-gcc \
    -DCMAKE_CXX_COMPILER=riscv32-linux-musl-g++ \
    -DCMAKE_C_COMPILER_WORKS=1 \
    -DCMAKE_CXX_COMPILER_WORKS=1 \
    -DCMAKE_MAKE_PROGRAM=make
make ws63_firmware
```

> 注意: WS63 构建需要将交叉工具链的 `bin/` 目录加入 `PATH`，并确保 `ws63_link_v4.exe` 可用。

## CMake 配置选项

| 选项 | 默认值 | 说明 |
|------|:------:|------|
| `CONVAI_PLATFORM` | `win10` (Windows) / `linux` (其他) | 目标平台: `linux` / `win10` / `ws63` |
| `CONVAI_USE_MBEDTLS` | `OFF` | 启用 mbedTLS 加密支持 |
| `CONVAI_BUILD_TESTS` | `OFF` | 构建测试 |

## 架构概览

```
┌──────────────────────────────────────────────────────────┐
│                     GoldieOS App                          │
│  ┌──────────┐  ┌───────────────┐  ┌───────────────────┐  │
│  │ 麦克风   │→ │ convai_bridge │→ │    ConvAI SDK     │  │
│  │ (录音)   │  │ PCM→G.711编码 │  │ protocol/base64   │  │
│  └──────────┘  └───────────────┘  │ WebSocket 发送     │  │
│                                    └────────┬──────────┘  │
│  ┌──────────┐  ┌───────────────┐           │             │
│  │ 扬声器   │← │ convai_bridge │← ┌────────↓──────────┐  │
│  │ (播放)   │  │ G.711→PCM解码 │  │    ConvAI SDK     │  │
│  └──────────┘  │ 缓冲区防欠载  │  │ base64 解码        │  │
│                └───────────────┘  │ WebSocket 接收     │  │
│                                    └───────────────────┘  │
└──────────────────────────────────────────────────────────┘
                          ↕ WebSocket
┌──────────────────────────────────────────────────────────┐
│                   Mock Server (Python)                    │
│  回声测试 / AI 对话服务                                    │
└──────────────────────────────────────────────────────────┘
```

### 音频管线

**录音通路 (上行):**
```
麦克风 → audio_read() [立体声 16-bit PCM]
  → Stereo→Mono 转换
  → G.711 A-law 编码 (20ms/帧)
  → Base64 → JSON → WebSocket → Server
```

**播放通路 (下行):**
```
Server → WebSocket → JSON → Base64 解码
  → G.711 A-law 解码 → PCM
  → 软件播放缓冲区 (2560 字节, 累积 ≥640 字节后写硬件)
  → audio_write() → 扬声器
```

## GoldieOS 应用结构

```
examples/goldieos/
├── apps/                    # 桌面应用 (Win10 通用 + WS63 专用)
│   ├── launcher/            # 桌面启动器 (WiFi/电池/星闪状态)
│   ├── settings/            # 系统设置 (WiFi/AI配置/关于)
│   ├── AItalk/              # AI 对话 (语音录制+动画)
│   ├── recorder/            # 录音机
│   ├── alarm/               # 闹钟
│   ├── animaton_player/     # 动画播放器
│   ├── charging_only/       # 充电界面
│   ├── shut_down/           # 关机界面
│   ├── Story/               # [WS63] 故事机
│   ├── Walkie-talkie/       # [WS63] 对讲机
│   └── dualscreen_ai/       # [WS63] 双屏AI
├── sdk_integration/         # ConvAI SDK 集成层
│   ├── convai_bridge.c/h    # 录音/播放/PTT 桥接 (平台自适应)
│   ├── convai_config.c/h    # 配置文件读取 (Win10)
│   └── convai_codec_g711a.c/h  # G.711 A-law 编解码
├── services/                # 平台服务实现
│   ├── alarm_service/       # 闹钟服务
│   ├── ntp_service/         # NTP 网络对时
│   └── aud_algo/            # 音频算法 (唤醒词检测)
├── drivers/                 # [WS63] 硬件驱动 (LCD/Touch/Codec/Key/Bat)
├── platform/ws63/           # [WS63] 平台初始化/存根/adc/sle
├── sle_service/             # [WS63] 星闪SLE服务
├── init/                    # 系统启动 (平台自适应)
├── compat/                  # 兼容适配层 (映射到 libwinvm.a)
├── include/                 # 头文件
│   ├── core/                # 框架基础设施 (13 个服务接口)
│   ├── gui/                 # tiny_gui 组件 (Win10)
│   ├── osal/                # OS 抽象层
│   ├── cplus_include/       # C++ 包装头 (WS63)
│   ├── goldie_osal/         # OSAL 实现头 (WS63)
│   ├── platform/win10/      # Win10 虚拟桌面
│   ├── platform/ws63/       # [WS63] 平台头 (BLE/SLE/SLP/BUS)
│   ├── system_res/          # [WS63] 系统资源 (音频/图片)
│   └── services/            # 业务服务 (alarm/ntp/audio)
├── third_party/             # 第三方开源库
│   ├── opus-1.6.1/          # Opus 音频编解码
│   ├── fatfs-R0.11/         # FAT 文件系统
│   ├── webrtc_vad/          # WebRTC 语音活动检测
│   ├── mbedtls/             # mbedTLS 加密
│   ├── tflite/              # TensorFlow Lite Micro
│   ├── helix/               # [WS63] MP3 解码
│   └── speex_aec/           # [WS63] 声学回声消除
└── libs/
    ├── win10/               # Win10 预编译库
    └── ws63/                # [WS63] 预编译库
```

## 开源依赖

| 组件 | 版本 | 许可证 | 用途 |
|------|------|--------|------|
| Opus | 1.6.1 | BSD 3-Clause | 音频编解码 |
| cJSON | 1.7.13 | MIT | JSON 解析 (SDK 核心) |
| mbedTLS | 3.1.0 | Apache 2.0 | TLS / AES 加密 |
| FatFs | R0.11 | BSD-style | SD 卡文件系统 |
| WebRTC VAD | - | BSD | 语音活动检测 |
| TensorFlow Lite Micro | - | Apache 2.0 | AI 推理引擎 |
| SpeexDSP | - | BSD | 声学回声消除 (仅 WS63) |
| lwIP | 2.1.3 | BSD | TCP/IP 协议栈 (仅 WS63) |
| Helix MP3 | - | RPSL/RCSL | MP3 解码 (仅 WS63) |

