# JVAV — 自定义 128-bit 指令集平台

JVAV 是一个完整的自研指令集平台，包含：

- **前端编译器**（`jvavc-front`）：`.jvl` 源码 → `.jvav` 汇编，C++17
- **后端汇编器/链接器**（`jvavc`）：`.jvav` 汇编 → `.bin` 字节码，C++17
- **虚拟机**（`jvm`）：执行 `.bin` 字节码，C99
- **反汇编器**（`disasm`）：静态反汇编 + 动态 trace，C99

全部工具链 100% 手写，不依赖任何第三方库（仅 CMake + STL）。

---

## 快速开始

### 构建

```bash
# 创建构建目录
mkdir build && cd build

# 生成 Makefile（MinGW）
cmake .. -G "MinGW Makefiles"

# 编译全部组件
mingw32-make -j$(nproc)
```

构建产物：

| 可执行文件 | 说明 |
|---|---|
| `jvavc-front` | 前端编译器 |
| `jvavc` | 后端汇编器/链接器 |
| `jvm` | 虚拟机 |
| `disasm` | 反汇编器 |
| `test_back` | 后端测试（12 项） |
| `test_front` | 前端测试（19 项） |

### 完整编译流程

```bash
# 1. 前端：.jvl → .jvav
./jvavc-front hello.jvl hello.jvav

# 2. 后端：.jvav → .bin（单文件）
./jvavc hello.jvav hello.bin

# 3. 运行
./jvm hello.bin
```

### 多文件链接

```bash
# 链接多个 .jvav 文件为单个 .bin
./jvavc math.jvav main.jvav -o program.bin
./jvm program.bin
```

---

## 项目结构

```
JVAV/
├── jvavc/
│   ├── front/              # 前端编译器
│   │   ├── include/        # AST / Lexer / Parser / Sema / CodeGen 头文件
│   │   └── src/            # 实现
│   ├── back/               # 后端汇编器与链接器
│   │   ├── include/        # Parser / Encoder / Linker 头文件
│   │   └── src/            # 实现
│   └── tools/              # 开发工具
│       └── src/disasm.c    # 反汇编器
├── jvm/                    # 虚拟机
│   ├── include/jvm.h       # VM 核心定义（128-bit 指令格式、寄存器、系统调用）
│   └── src/                # 执行引擎 + 主程序
├── tests/
│   ├── jvavc/back/         # 后端自动化测试（12 项）
│   ├── jvavc/front/        # 前端自动化测试（19 项）
│   └── samples/            # 测试用例样本
└── CMakeLists.txt          # 根级 CMake
```

---

## JVL 语言语法

JVAV 前端语言是一种类 C 的静态类型语言，编译到 JVAV 汇编。

### 基本类型

| 类型 | 说明 |
|---|---|
| `int` | 128-bit 有符号整数 |
| `char` | 128-bit 字符（字大小） |
| `bool` | 布尔值 |
| `void` | 无返回值 |
| `ptr<T>` | 指向 T 类型的指针 |
| `array<T>` | T 类型数组 |

### 示例程序

```jvl
func add(a: int, b: int): int {
    return a + b;
}

func main(): int {
    var x: int = 5;
    var y: int = 3;
    var z: int = add(x, y);
    putint(z);
    putchar(10);    // 换行
    return 0;
}
```

### 控制流

```jvl
func max(a: int, b: int): int {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

func sum(n: int): int {
    var s: int = 0;
    var i: int = 1;
    while (i <= n) {
        s = s + i;
        i = i + 1;
    }
    return s;
}
```

### 模块导入

```jvl
// math.jvl
func factorial(n: int): int {
    if (n <= 1) { return 1; }
    return n * factorial(n - 1);
}
```

```jvl
// main.jvl
import "math.jvl";

func main(): int {
    putint(factorial(5));
    return 0;
}
```

`import` 支持递归模块导入和循环导入防护。

### 内置函数

| 函数 | 说明 |
|---|---|
| `putint(x)` | 输出整数到 stdout |
| `putchar(c)` | 输出字符到 stdout |

---

## JVAV 汇编语法

JVAV 汇编是类 ARM 风格的文本汇编，直接对应 128-bit 指令。

### 寄存器

| 寄存器 | 编号 | 用途 |
|---|---|---|
| R0–R5 | 0–5 | 通用寄存器 / 函数参数 |
| R6 | 6 | 帧指针 (FP) |
| R7 | 7 | 保留（后端临时寄存器） |
| PC | 8 | 程序计数器 |
| SP | 9 | 栈指针 |
| FLAGS | 10 | 标志寄存器（ZF 等） |

### 指令集

| 指令 | 格式 | 说明 |
|---|---|---|
| `MOV Rd, Rs` | `Rd = Rs` | 寄存器传送 |
| `LDR Rd, [Rs]` | `Rd = mem[Rs]` | 寄存器间接加载 |
| `LDR Rd, [imm]` | `Rd = mem[imm]` | 直接寻址加载（伪指令） |
| `STR [Rs], Rd` | `mem[Rs] = Rd` | 寄存器间接存储 |
| `STR [imm], Rd` | `mem[imm] = Rd` | 直接寻址存储（伪指令） |
| `ADD Rd, Rs, Rt` | `Rd = Rs + Rt` | 加法 |
| `SUB Rd, Rs, Rt` | `Rd = Rs - Rt` | 减法 |
| `MUL Rd, Rs, Rt` | `Rd = Rs * Rt` | 乘法 |
| `DIV Rd, Rs, Rt` | `Rd = Rs / Rt` | 除法 |
| `CMP Rs, Rt` | `FLAGS = Rs - Rt` | 比较 |
| `JMP label` | `PC = label` | 无条件跳转（伪指令） |
| `JZ label` | `if ZF` 跳转 | 为零跳转 |
| `JNZ label` | `if !ZF` 跳转 | 非零跳转 |
| `JE/JNE/JL/JG/JLE/JGE label` | 条件跳转 | 扩展条件跳转 |
| `PUSH Rs` | `mem[--SP] = Rs` | 入栈 |
| `POP Rd` | `Rd = mem[SP++]` | 出栈 |
| `CALL label` | 函数调用（伪指令） | 保存返回地址，跳转 |
| `RET` | 函数返回 | 恢复 PC |
| `LDI Rd, imm` | `Rd = imm` | 加载立即数 |
| `HALT` | 停机 | 终止执行 |

### 伪指令与数据

| 伪指令 | 说明 |
|---|---|
| `.global name` | 导出符号 |
| `.extern name` | 声明外部符号 |
| `LABEL: EQU value` | 常量定义 |
| `#include "file"` | 文件包含 |
| `DB b1, b2, ...` | 定义字节数据（字对齐） |
| `DW w1, w2, ...` | 定义字数据 |
| `DT t1, t2, ...` | 定义 128-bit 数据 |

### 汇编示例

```asm
    .global _start
_start:
    LDI R0, msg
    LDI R1, 14
    LDI R2, 1
    LDI R4, 0
    LDI R6, 0xFFF0      ; putchar I/O port
pstr:
    LDR R7, [R0]        ; load char
    STR [R6], R7        ; output char
    ADD R0, R0, R2      ; next char
    SUB R1, R1, R2      ; count--
    CMP R1, R4
    JNZ pstr
    HALT
msg: DB 72,101,108,108,111,44,32,87,111,114,108,100,33,10
```

---

## 虚拟机架构

### 128-bit 指令格式

每条指令固定 16 字节（128 bit）：

```
[ imm_high(32) | imm_low(64) | src2(8) | src1(8) | dst(8) | op(8) ]
   MSB                                                            LSB
```

立即数通过 `imm_low`（64 bit）+ `imm_high`（32 bit）组合为 96 bit 有符号值。

### 内存与 I/O

- 内存以 **128-bit 字**为单位寻址
- 初始 4096 字，按需动态扩容至 ~16 GB
- I/O 端口映射（`STR [port], value`）：
  - `0xFFF0` — `putchar`
  - `0xFFF2` — `putint`

### 系统调用

通过 `SYSCALL_CMD` 端口（`0xFFE0`）触发，支持：

| 调用号 | 功能 |
|---|---|
| `SYS_MMAP_FILE` | 内存映射文件 |
| `SYS_FOPEN/FREAD/FWRITE/FCLOSE` | 文件操作 |
| `SYS_MEMCPY/MEMSET` | 内存操作 |

---

## 反汇编器

```bash
# 静态反汇编
./disasm program.bin

# 动态 trace（模拟执行并标记已执行指令）
./disasm -t program.bin
```

Trace 模式内嵌精简 VM，单步执行并标注 `>>>` 已执行指令，未被命中的字按 `DB` 输出。

---

## 测试

```bash
# 后端测试（12 项）
./test_back
# 输出：12 tests passed, 0 failed

# 前端测试（19 项）
./test_front
# 输出：19 tests passed, 0 failed
```

测试覆盖：
- **前端**：Lexer（4）、Parser（5）、Sema（4）、CodeGen（4）、Integration（2）
- **后端**：Parser（5）、Encoder（5）、Integration（2）

---

## 技术要点

| 特性 | 实现细节 |
|---|---|
| 128-bit 指令 | `typedef __int128 var`，`#pragma pack(1)` 的 16 字节结构体 |
| 栈帧约定 | R6=FP，前 4 参数走寄存器，其余压栈 |
| 条件跳转 | 前端生成 JE/JNE/JL/JG/JLE/JGE，VM 原生支持 |
| 立即数扩展 | 后端 `LDI` + 临时寄存器自动处理大立即数和 label 跳转 |
| 多文件链接 | 结构化拼接（非文本拼接），EQU 全局收集，基地址重定位 |
| 循环导入防护 | `importedFiles` 集合去重 |

---

## 许可证

MIT License
