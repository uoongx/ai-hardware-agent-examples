# AI Hardware Agent Examples 最佳实践

本文档基于华为 AI Hardware Agent SDK，指导开发者完成各平台的编译、烧录与运行。

---

## 目录

1. [概述](#概述)
2. [前置环境准备](#前置环境准备)
3. [SDK 与 Examples 获取](#sdk-与-examples-获取)
4. [工程集成](#工程集成)
5. [设备凭证配置](#设备凭证配置)
6. [平台编译指南](#平台编译指南)
   - [WS63 平台](#ws63-平台)

---

## 概述

AI Hardware Agent SDK 采用 **SDK 与 Examples 分离发布** 的模式：

| 组件 | 说明 | 发布形式 |
|------|------|----------|
| **AI Hardware Agent SDK** | 核心库（含 `libconvai_sdk.a` + 公共头文件） | `ai-hardware-agent-sdk-<version>.tar` |
| **Examples** | 平台示例代码（含 GoldieOS demo 应用） | `ai-hardware-agent-examples-<version>.tar` |

SDK 的 `include/` 和 `libs/` 放在工程根目录，供各平台共用。

最终集成后的目录结构：

```
ai-hardware-agent-project/
├── include/convai/                         # SDK 公共头文件（来自 SDK 包）
│   ├── convai_api.h
│   ├── convai_event.h
│   ├── convai_platform.h
│   └── convai_types.h
├── libs/                                    # SDK 预编译库（来自 SDK 包）
│   └── ws63/
│       └── libconvai_sdk.a
├── CMakeLists.txt                          # 顶层构建脚本（来自 Examples 包）
├── examples/
│   └── goldieos/
│       ├── CMakeLists.txt
│       ├── sdk_integration/                # SDK 集成桥接层
│       │   ├── convai_bridge.c/h
│       │   ├── convai_config.c/h
│       │   └── convai_codec_g711a.c/h
│       ├── apps/                           # 应用层
│       ├── services/                       # 系统服务
│       ├── drivers/                        # 硬件驱动
│       ├── platform/ws63/                  # WS63 平台适配
│       ├── platform/convai_platform_ws63.c # AI Hardware Agent HAL 实现
│       ├── include/                        # GoldieOS 内部头文件
│       ├── third_party/                    # 第三方库（Opus, FatFs, mbedTLS, TFLite 等）
│       ├── init/                           # 系统初始化
│       ├── compat/                         # 兼容层
│       └── tools/                          # 工具集
│           ├── build/                      #   构建工具
│           │   ├── tools/
│           │   │   ├── compiler/riscv/     #     交叉编译工具链
│           │   │   └── ws63_link_v4.exe    #     固件链接器
│           │   └── config/ws63/            #     链接脚本 & 配置文件
│           └── burn/                       #   烧录工具
│               └── hisi/
│                   └── BurnTool_5.0.39/    #     BurnTool GUI
└── tools/                                  # 辅助工具（来自 Examples 包）
```

---

## 前置环境准备

| 工具 | 版本要求 | 说明 |
|------|----------|------|
| CMake | ≥ 3.10 | 构建配置 |
| MinGW Makefiles | — | Windows 下使用 `mingw32-make` |
| RISC-V 交叉编译工具链 | GCC 7.3.0 | 已包含在 Examples 包中 |

Windows 下推荐使用 [MSYS2](https://www.msys2.org/)。

---

## SDK 与 Examples 获取

### 下载 SDK 包

从以下地址下载 AI Hardware Agent SDK 发布包：

> **下载地址：** `https://download.huaweicloud-koophone.com/ai-hardware-agent-sdk/ai-hardware-agent-sdk.26.6.0.tar`

SDK 包内容：

```
ai-hardware-agent-sdk-26.6.0.tar
├── include/convai/              # 公共 API 头文件
│   ├── convai_api.h
│   ├── convai_event.h
│   ├── convai_platform.h
│   └── convai_types.h
└── libs/
    └── ws63/
        └── libconvai_sdk.a      # WS63 平台预编译静态库
```

### 下载 Examples 包

从以下地址下载示例代码包：

> **下载地址：** `https://github.com/uoongx/ai-hardware-agent-examples.git`

Examples 包内容：

```
ai-hardware-agent-examples
├── CMakeLists.txt               # 顶层构建脚本
├── examples/
│   └── goldieos/                # GoldieOS 综合示例
│       ├── CMakeLists.txt
│       ├── apps/                # 应用层
│       ├── sdk_integration/     # SDK 集成桥接层
│       ├── services/            # 系统服务
│       ├── drivers/             # 硬件驱动
│       ├── platform/ws63/       # WS63 平台适配
│       ├── platform/            # AI Hardware Agent HAL 实现
│       ├── include/             # GoldieOS 内部头文件
│       ├── third_party/         # 第三方库（Opus, FatFs, mbedTLS, TFLite 等）
│       ├── tools/               # 工具集
│       │   ├── build/           #   构建工具（交叉编译工具链 + ws63_link_v4.exe）
│       │   └── burn/            #   烧录工具
│       ├── init/                # 系统初始化
│       └── compat/              # 兼容层
└── tools/                       # 辅助工具
```

---

## 工程集成

### 步骤 1：创建工作目录并解压 Examples 包

```bash
mkdir ai-hardware-agent-examples
cd ai-hardware-agent-examples
tar -xvf /path/to/ai-hardware-agent-examples.tar
```

解压后得到 `CMakeLists.txt`、`examples/`、`tools/`。

### 步骤 2：将 SDK 包解压到工程根目录

SDK 的 `include/` 和 `libs/` 放在工程根目录：

```bash
tar -xvf /path/to/ai-hardware-agent-sdk-26.6.0.tar
```

### 步骤 3：验证集成结果

```bash
# 确认 SDK 文件在根目录
ls include/convai/convai_api.h              # SDK 公共头文件
ls libs/ws63/libconvai_sdk.a                # WS63 SDK 静态库

# 确认桥接层文件完整
ls examples/goldieos/sdk_integration/convai_bridge.c
ls examples/goldieos/sdk_integration/convai_config.c
ls examples/goldieos/sdk_integration/convai_codec_g711a.c

# 确认顶层构建脚本
ls CMakeLists.txt
```

---

## 设备凭证配置

编译前，需要将设备的五元组信息配置到 `convai_bridge.c` 中。

### 准备凭证信息

联系平台获取以下凭证：

| 参数 | 说明 | 示例 |
|------|------|------|
| `agent_id` | 智能体 ID | `"goldieos-agent"` |
| `product_id` | 产品 ID | `"your_product_id"` |
| `product_key` | 产品密钥 | `"your_product_key"` |
| `product_secret` | 产品密钥（加密用） | `"your_product_secret"` |
| `device_name` | 设备名称 | `"goldieos-ws63"` |

### 修改默认配置

在 `examples/goldieos/sdk_integration/convai_bridge.c` 中找到默认配置宏定义，替换为实际值：

```c
/* ---- default config ---- */
#define BRIDGE_DEFAULT_BOT_ID         "your_agent_id"       // ← 替换为实际的 agent_id
#define BRIDGE_DEFAULT_PRODUCT_ID     "your_product_id"     // ← 替换为实际的 product_id
#define BRIDGE_DEFAULT_PRODUCT_KEY    "your_product_key"    // ← 替换为实际的 product_key
#define BRIDGE_DEFAULT_PRODUCT_SECRET "your_product_secret" // ← 替换为实际的 product_secret
#define BRIDGE_DEFAULT_DEVICE_NAME    "your_device_name"    // ← 替换为实际的 device_name
```

WS63 平台在 `convai_bridge_start()` 中直接使用这些默认值构建连接配置。

> **注意：** 编译前务必确认凭证已替换为有效值，否则设备将无法连接平台服务。

---

## 平台编译指南

### WS63 平台

#### 交叉编译工具链

交叉编译工具链已包含在 Examples 包中，位于：

```
examples/goldieos/tools/build/tools/compiler/riscv/cc_riscv32_musl_105/cc_riscv32_musl_fp_win/bin/
├── riscv32-linux-musl-gcc.exe      # RISC-V C 交叉编译器
├── riscv32-linux-musl-g++.exe      # RISC-V C++ 交叉编译器
└── ...
```

使用时将上述 `bin/` 目录加入 `PATH`：

```bash
export PATH="/path/to/ai-hardware-agent-examples/examples/goldieos/tools/build/tools/compiler/riscv/cc_riscv32_musl_105/cc_riscv32_musl_fp_win/bin:$PATH"
```

验证安装：

```bash
riscv32-linux-musl-gcc --version
riscv32-linux-musl-g++ --version
```

#### 链接工具

固件链接器 `ws63_link_v4.exe` 已包含在 Examples 包中，位于：

```
examples/goldieos/tools/build/tools/ws63_link_v4.exe
```

在 `examples/goldieos/CMakeLists.txt` 中已预配置好路径：

```cmake
# 优先级: CMake 参数 > 环境变量 > 相对路径回退
if(WS63_BOARD_ROOT)
    set(WS63_LINK_ROOT "${WS63_BOARD_ROOT}")
elseif(DEFINED ENV{GOLDIEOS_ROOT})
    set(WS63_LINK_ROOT "$ENV{GOLDIEOS_ROOT}")
else()
    set(WS63_LINK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

set(WS63_LINKER "${WS63_LINK_ROOT}/tools/build/tools/ws63_link_v4.exe")
```

如果你的 `ws63_link_v4.exe` 不在 `examples/goldieos/tools/build/tools/` 下，可通过以下方式覆盖：

```bash
# CMake 参数
cmake .. -DWS63_BOARD_ROOT=/path/to/your/goldieos
```

#### CMake 配置说明

顶层 `CMakeLists.txt` 中，WS63 平台的 `convai_sdk` 使用 `IMPORTED STATIC` 方式链接预编译的 `libconvai_sdk.a`：

```cmake
add_library(convai_sdk STATIC IMPORTED)
set_target_properties(convai_sdk PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/ws63/libconvai_sdk.a
)
```

固件链接通过 `ws63_link_v4.exe` 将应用库、Opus 库、SDK 库三者链接：

```
libgoldieos_ws63.a  ──┐
libopus.a           ──┤── ws63_link_v4.exe ──→ goldieos.fwpkg
libconvai_sdk.a     ──┘
```

### 编译

在工程根目录下执行：

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles" \
    -DCONVAI_PLATFORM=ws63 \
    -DCMAKE_C_COMPILER=riscv32-linux-musl-gcc \
    -DCMAKE_CXX_COMPILER=riscv32-linux-musl-g++ \
    -DCMAKE_C_COMPILER_WORKS=1 \
    -DCMAKE_CXX_COMPILER_WORKS=1 \
    -DCMAKE_MAKE_PROGRAM=mingw32-make
mingw32-make
```

| 参数 | 说明 |
|------|------|
| `-G "MinGW Makefiles"` | 使用 MinGW Makefiles 生成器 |
| `-DCONVAI_PLATFORM=ws63` | 指定目标平台为 WS63 |
| `-DCMAKE_C_COMPILER=riscv32-linux-musl-gcc` | RISC-V C 交叉编译器 |
| `-DCMAKE_CXX_COMPILER=riscv32-linux-musl-g++` | RISC-V C++ 交叉编译器 |
| `-DCMAKE_C_COMPILER_WORKS=1` | 跳过编译器可用性检测（交叉编译必需） |
| `-DCMAKE_CXX_COMPILER_WORKS=1` | 跳过 C++ 编译器可用性检测 |
| `-DCMAKE_MAKE_PROGRAM=mingw32-make` | 指定 make 程序 |

编译产物位于 `build/examples/goldieos/out/`：

```
build/examples/goldieos/out/
├── goldieos.fwpkg     # 固件包（用于烧录）
├── goldieos.bin       # 固件二进制文件
└── goldieos.elf       # ELF 文件（含调试符号）
```

#### 烧录

烧录使用 BurnTool，位于：

```
examples/goldieos/tools/burn/hisi/BurnTool_5.0.39/BurnTool/BurnTool.exe
```

**烧录步骤：**

1. **启动 BurnTool** — 双击 `BurnTool.exe`
2. **配置参数** — 选择 COM 口 → 设置波特率（推荐 `921600`）
3. **加载固件** — 点击"Select file"，选择 `build/examples/goldieos/out/goldieos.fwpkg`
4. **进入 ISP 模式** — 关机状态下按住 RESET → 接上 USB → 松开 RESET
5. **执行烧录** — 点击"烧录"，等待完成
6. **运行固件** — 按 RESET 或重新上电

**硬件连接：** 使用 USB 转串口线连接 WS63 开发板，确认串口驱动已安装。

---

> **版本信息：** 本文档基于 AI Hardware Agent SDK 26.6.0 版本编写。
