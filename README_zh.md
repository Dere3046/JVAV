# JVAV 程序设计语言

> **声明：** 本项目仅为玩笑/恶搞性质。JVAV 由「张浩杨博士」提出，由 **Dere3046** 实现。请不要对张浩杨或任何与 JVAV 相关的人进行骚扰、攻击或恶意中伤。本项目纯粹为了娱乐和教育目的而存在。

JVAV 是一个从零开始构建的完整自定义程序设计语言及工具链，包含 128 位指令集架构、类 C 前端语言（JVL）、类 ARM 后端汇编（JVAV）、虚拟机与反汇编器。

[English Version](./README.md)

---

## 快速开始

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
mingw32-make -j$(nproc)
```

### 编译与运行

```bash
# 前端：.jvl -> .jvav
./jvavc-front hello.jvl hello.jvav

# 后端：.jvav -> .bin
./jvavc hello.jvav hello.bin

# 运行
./jvm hello.bin
```

### 多文件链接

```bash
./jvavc math.jvav main.jvav -o program.bin
./jvm program.bin
```

---

## 项目结构

```
JVAV/
├── jvavc/
│   ├── front/       # 前端编译器 (.jvl -> .jvav), C++17
│   ├── back/        # 后端汇编器与链接器 (.jvav -> .bin), C++17
│   └── tools/       # 反汇编器 (静态 + trace), C99
├── jvm/             # 虚拟机执行引擎, C99
├── tests/           # 自动化测试 (后端: 12, 前端: 25)
└── docs/            # 详细文档
```

---

## 核心特性

- **128 位指令格式** — 每条指令固定 16 字节
- **JVL 语言** — 类 C 语法，支持函数、变量、控制流、`import` 模块
- **JVAV 汇编** — 类 ARM 文本汇编，支持伪指令
- **米米世界所有权** — 受 Rust 启发的所有权、移动与借用检查 (`&x`, `&mut x`)
- **多文件链接器** — 结构化链接，支持 EQU 全局收集与基地址重定位
- **导入系统** — 递归模块导入，带循环导入防护
- **反汇编器** — 支持静态反汇编与动态 trace 模式

---

## 文档

| 文档 | 说明 |
|------|------|
| [docs/frontend_syntax.md](docs/frontend_syntax.md) | JVL 语言语法、类型、所有权 |
| [docs/backend_syntax.md](docs/backend_syntax.md) | JVAV 汇编语法与指令集 |
| [docs/jvm_features.md](docs/jvm_features.md) | 虚拟机架构、内存模型、系统调用 |
| [docs/mimiworld_ownership.md](docs/mimiworld_ownership.md) | 米米世界所有权与借用检查器 |

---

## 测试

```bash
./test_back    # 12 项后端测试
./test_front   # 25 项前端测试
```

---

## 许可证

MIT License
