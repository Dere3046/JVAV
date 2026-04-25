# JVAV 程序设计语言

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![CI](https://github.com/Dere3046/JVAV/actions/workflows/ci.yml/badge.svg)](https://github.com/Dere3046/JVAV/actions)
[![CI-Android](https://github.com/Dere3046/JVAV/actions/workflows/ci-android.yml/badge.svg)](https://github.com/Dere3046/JVAV/actions)
[![Stars](https://img.shields.io/github/stars/Dere3046/JVAV?style=social)](https://github.com/Dere3046/JVAV)
![JVAV](https://img.shields.io/badge/JVAV-128--位-ff69b4.svg)
![C/C++](https://img.shields.io/badge/C%2FC%2B%2B-99%2F17-blue.svg)

> **声明：** 本项目仅为玩笑/恶搞性质。JVAV 由「张浩杨博士」提出，由 **Dere3046** 实现。请不要对张浩杨或任何与 JVAV 相关的人进行骚扰、攻击或恶意中伤。本项目纯粹为了娱乐和教育目的而存在。

JVAV 是一个从零开始构建的完整自定义程序设计语言及工具链，包含 128 位指令集架构、类 C 前端语言（JVL）、类 ARM 后端汇编（JVAV）、虚拟机与反汇编器。

[English Version](./README.md)

---

## 编译与运行

### 传统流水线

```bash
# 前端：.jvl -> .jvav
./jvlc hello.jvl hello.jvav

# 后端：.jvav -> .bin
./jvavc hello.jvav hello.bin

# 运行
./jvm hello.bin
```

### 一键编译并运行

```bash
# 编译 .jvl -> .jvav -> .bin 并执行，自动清理中间文件
./jvlc --run hello.jvl

# 编译并运行，保留最终二进制文件
./jvlc --run hello.jvl -o hello.bin
```

### 多文件链接

```bash
./jvavc math.jvav main.jvav -o program.bin
./jvm program.bin
```

---

## 打包

```bash
cpack -C Release
```

生成平台相关安装包（`JVAV-x.x.x-win64.zip`、`JVAV-x.x.x-Linux.tar.gz` 等）。

---

## 项目结构

```
JVAV/
├── jvavc/
│   ├── front/       # 前端编译器 (.jvl -> .jvav), C++17
│   ├── back/        # 后端汇编器与链接器 (.jvav -> .bin), C++17
│   └── tools/       # 反汇编器 (静态 + trace), C99
├── jvm/             # 虚拟机执行引擎, C99
├── std/             # 标准库 (io, math, mem, string)
├── benchmark/       # 性能基准测试套件 (Python)
├── tests/           # 自动化测试 (后端: 98, 前端: 107)
└── docs/            # 详细文档
```

---

## 核心特性

- **128 位指令格式** — 每条指令固定 16 字节，支持算术与位运算（AND、OR、XOR、SHL、SHR、NOT）
- **JVL 语言** — 类 C 语法，支持函数、变量、控制流、`import` 模块
- **JVAV 汇编** — 类 ARM 文本汇编，支持伪指令
- **米米世界所有权** — 受 Rust 启发的所有权、移动与借用检查 (`&x`, `&mut x`)
- **标准库** — `std/io`、`std/math`、`std/mem`、`std/string`，自动 import 路径解析
- **多文件链接器** — 结构化链接，支持 EQU 全局收集与基地址重定位
- **导入系统** — 递归模块导入，带循环导入防护
- **反汇编器** — 支持静态反汇编与动态 trace 模式
- **Rust 风格诊断信息** — 错误代码、源代码片段、帮助提示
- **跨平台** — Linux、Windows 与 Android（x86、x64、ARM、ARM64）；静态链接二进制文件；GitHub Actions 多架构矩阵 CI
- **PATH 就绪** — `std/` 目录与 `bin/` 一起分发；将 `bin/` 加入 PATH 即可从任意位置使用 `jvlc`/`jvavc`/`jvm`/`disasm`
- **版本号标志** — 所有工具均支持 `-v` / `--version`

---

## 文档

| 文档 | 说明 |
|------|------|
| [docs/frontend_syntax.md](docs/frontend_syntax.md) | JVL 语言语法、类型系统、推导、所有权 |
| [docs/backend_syntax.md](docs/backend_syntax.md) | JVAV 汇编语法与指令集 |
| [docs/jvm_features.md](docs/jvm_features.md) | 虚拟机架构、内存模型、系统调用 |
| [docs/mimiworld_ownership.md](docs/mimiworld_ownership.md) | 米米世界所有权与借用检查器 |
| [docs/stdlib.md](docs/stdlib.md) | 标准库 API 与使用说明 |

---

## 测试

```bash
# 通过 CTest 运行所有测试
ctest --output-on-failure

# 或直接运行测试二进制文件
./test_back    # 98 项后端单元 + 集成测试
./test_front   # 107 项前端单元 + 集成测试
```

测试覆盖范围：
- **词法分析器**：关键字、数字、字符串、字符、注释、符号、CRLF 兼容、错误
- **语法分析器**：所有类型、控制流、运算符、优先级、借用、导入、错误恢复
- **语义分析**：类型推导、所有权、借用冲突、未初始化变量、作用域
- **代码生成**：函数序言/尾声、局部变量、调用、控制流、指针、字符串
- **后端**：所有指令、EQU、.global/.extern、DB/DW/DT、#include、编码、链接
- **集成测试**：端到端编译运行，覆盖算术、位运算、控制流、堆内存、递归、导入、全局变量、标准库、版本号标志、缺失标准库诊断、Rust 风格错误信息

---

## 贡献者

| 名称 | 贡献 |
|------|------|
| Dr.zhanghaoyang | 提出 JVAV 语言 |
| Dere | 初始实现 |
| Derry | 基于原型使用 Claude AI 快速做出大量改进 |
| Claude AI | 协助 Derry 进行快速开发与改进 |
| Moonshot AI (Kimi) | 文档协助 |

---

## 许可证

MIT License
