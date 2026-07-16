# WS63 固件构建指南

## 环境要求

- RISC-V 交叉编译器 (`riscv32-linux-musl-gcc`) 在 PATH 中
- CMake 3.10+
- Python 3.x
- MSYS2/MinGW 环境

## Board 包路径配置

构建系统会自动查找 ws63 board 包（goldie-os_pd001），优先顺序：

1. CMake 参数 `-DWS63_BOARD_ROOT=...`
2. 环境变量 `GOLDIEOS_ROOT`
3. 默认相对路径 `../../../../godie/goldie-os_pd001`

### 方式1：设置环境变量（推荐）

```bash
# Windows
set GOLDIEOS_ROOT=D:\koophone\godie\goldie-os_pd001
# 或永久设置
setx GOLDIEOS_ROOT D:\koophone\godie\goldie-os_pd001 /M
```

### 方式2：构建时传参

```bash
cmake -DWS63_BOARD_ROOT=D:\koophone\godie\goldie-os_pd001 ..
```

## 构建步骤

```bash
# 1. 进入项目根目录
cd D:/koophone/aiHard/convai-sdk

# 2. 创建构建目录
mkdir -p build_ws63
cd build_ws63

# 3. CMake 配置
cmake -G "MinGW Makefiles" ^
      -DCONVAI_PLATFORM=ws63 ^
      -DCMAKE_C_COMPILER=riscv32-linux-musl-gcc ^
      -DCMAKE_C_COMPILER_WORKS=1 ^
      -DCMAKE_MAKE_PROGRAM=make ^
      ..

# 4. 构建
cmake --build .

# 5. 输出文件位于
#    build_ws63/examples/ws63/out/goldieos.fwpkg
#    build_ws63/examples/ws63/out/goldieos.bin
#    build_ws63/examples/ws63/out/goldieos.elf
```

## 快速构建（一行命令）

```bash
cd D:/koophone/aiHard/convai-sdk && rm -rf build_ws63 && mkdir build_ws63 && cd build_ws63 && cmake -G "MinGW Makefiles" -DCONVAI_PLATFORM=ws63 -DCMAKE_C_COMPILER=riscv32-linux-musl-gcc -DCMAKE_C_COMPILER_WORKS=1 -DCMAKE_MAKE_PROGRAM=make .. && cmake --build .
```



### 项目依赖

- `convai_sdk` 库（从 src/ 目录构建）
- `opus` 音频编解码库
- ws63 board 库（由 ws63_link_v4.exe 自动链接）

## 输出文件说明

| 文件 | 大小 | 说明 |
|------|------|------|
| goldieos.fwpkg | ~1.8MB | 最终固件包（含签名） |
| goldieos.bin | ~1.7MB | 原始二进制固件 |
| goldieos.elf | ~11MB | 调试用 ELF 文件（含符号） |
| goldieos_sign.bin | ~1.7MB | 签名后的二进制 |

## 清理构建

```bash
cd D:/koophone/aiHard/convai-sdk
rm -rf build_ws63
```

## 已知问题

1. 构建目录可能被占用，清理时确保没有进程在使用
2. ws63_link_v4.exe 位于 `goldie-os_pd001/tools/build/tools/`