# JVAV Linker

The JVAV linker combines multiple `.jvav` assembly files into a single executable binary. It resolves cross-file symbol references, performs address relocation, and produces a flat binary output.

## Linking Model

JVAV uses a simple static linking model:

- Multiple object files (`.jvav`) are parsed independently
- Symbol tables are built for each file
- Cross-file references are resolved using `.global` and `.extern` declarations
- The output is a single flat binary (`.bin`)

There is no dynamic linking, shared libraries, or relocation metadata. The linker performs all resolution at build time.

## Linking Command

```bash
jvavc main.jvav lib.jvav -o program.bin
```

Multiple input files are specified in order. Each file is parsed, assembled, and then linked with the others.

## Linking Process

### Phase 1: Parsing

Each input file is parsed independently:

1. Textual preprocessing (`#include` directives are resolved)
2. Instructions, labels, and directives are parsed
3. EQU symbols are collected
4. `.global` and `.extern` declarations are recorded

### Phase 2: Symbol collection

The linker collects symbols from all input files:

- **Defined symbols**: Labels and `.global` exports from each file
- **Undefined symbols**: Labels referenced but not defined (must be resolved by `.global` from another file or declared `.extern`)
- **EQU symbols**: Compile-time constants (file-local unless explicitly shared)

### Phase 3: Address assignment

Each file is assigned a base address in the final binary:

```
File 1: base address 0
File 2: base address = size of File 1
File 3: base address = size of File 1 + size of File 2
...
```

Labels within each file are resolved relative to the file's base address.

### Phase 4: Symbol resolution

For each undefined label:

1. Search all files for a `.global` symbol with the same name
2. If found, replace the reference with the symbol's absolute address
3. If not found and the symbol is declared `.extern`, leave it as zero (or report a warning)
4. If not found and not declared `.extern`, emit a linker error

### Phase 5: Encoding and output

All instructions are encoded into the 16-byte binary format. The encoded binaries from all files are concatenated in order and written to the output file.

## Symbol Visibility Rules

### .global symbols

Exported by one file, usable by others:

```asm
; math.jvav
    .global add
add:
    PUSH R6
    PUSH R1
    PUSH R2
    PUSH R3
    MOV  R6, SP
    LDR R0, [FP+5]
    LDR R1, [FP+6]
    ADD R0, R0, R1
    MOV SP, R6
    POP R3
    POP R2
    POP R1
    POP R6
    RET
```

```asm
; main.jvav
    .global _start
    .extern add
_start:
    LDI R0, 3
    LDI R1, 4
    PUSH R1
    PUSH R0
    CALL add
    LDI R4, 2
    ADD SP, SP, R4
    HALT
```

### .extern symbols

Declared but not defined in the current file:

```asm
    .extern printf
    .extern some_lib_function
```

Declaring a symbol `.extern` suppresses the "undefined label" error. If the symbol is not defined in any linked file, the reference is unresolved (typically zero-filled).

### Local symbols

Labels not marked `.global` are local to their file and cannot be referenced from other files.

```asm
; Local label — not visible outside this file
loop:
    CMP R0, R1
    JE done
    JMP loop
done:
```

## Entry Point

Every executable must have a `_start` label that is placed at address 0:

```asm
    .global _start
_start:
    CALL main
    HALT
```

When using the JVL frontend, `_start` is automatically generated before `main()`.

## Multi-File Example

### File: math.jvav
```asm
    .text
    .global add
    .global mul

add:
    PUSH R6
    PUSH R1
    PUSH R2
    PUSH R3
    MOV  R6, SP
    LDR R0, [FP+5]
    LDR R1, [FP+6]
    ADD R0, R0, R1
    MOV SP, R6
    POP R3
    POP R2
    POP R1
    POP R6
    RET

mul:
    PUSH R6
    PUSH R1
    PUSH R2
    PUSH R3
    MOV  R6, SP
    LDR R0, [FP+5]
    LDR R1, [FP+6]
    MUL R0, R0, R1
    MOV SP, R6
    POP R3
    POP R2
    POP R1
    POP R6
    RET
```

### File: main.jvav
```asm
    .text
    .global _start
    .extern add
    .extern mul

_start:
    LDI R0, 3
    LDI R1, 4
    PUSH R1
    PUSH R0
    CALL add
    LDI R4, 2
    ADD SP, SP, R4
    
    PUSH R0         ; result of add
    LDI R0, 2
    PUSH R0
    CALL mul
    LDI R4, 2
    ADD SP, SP, R4
    
    HALT
```

### Linking
```bash
jvavc main.jvav math.jvav -o program.bin
```

## EQU and Linking

EQU symbols are resolved at assembly time and are not visible to the linker:

```asm
; File A
BUFFER_SIZE: EQU 1024
    .global buffer
buffer: DT 0

; File B cannot reference BUFFER_SIZE
; (unless included via #include, not linking)
```

To share constants across files, use `#include` for textual inclusion or define the constant in each file.

## Error Messages

### Undefined symbol

```
error: undefined symbol `foo`
  referenced in: main.jvav:10
```

Fix: Define the symbol in one file and mark it `.global`, or declare it `.extern` if defined externally.

### Multiple definitions

```
error: multiple definitions of symbol `bar`
  defined in: file1.jvav:5
  defined in: file2.jvav:8
```

Fix: Rename one of the symbols or make them local (remove `.global`).

### Missing entry point

```
error: no entry point `_start` defined
```

Fix: Add a `_start` label and mark it `.global`.

## Linking vs #include

| Feature | `#include` | Multi-file linking |
|---------|------------|-------------------|
| Timing | Parse time | Link time |
| Scope | Single namespace | Per-file namespaces |
| Labels | Shared | Private by default |
| Use case | Shared constants/macros | Separate compilation |
| Binary | One object file | Multiple object files |

Use `#include` when:
- Sharing EQU constants
- Sharing macro-like code snippets
- You want a single compilation unit

Use multi-file linking when:
- Compiling separate modules independently
- Building libraries with defined interfaces
- You want encapsulation between files
