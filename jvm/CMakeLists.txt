# Detect architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
    set(JIT_EMIT_SRC src/jit_emit_arm64.c)
    set(JIT_BACKEND_SRC src/jit_arm64.c)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
    # ARM32 JIT not yet implemented; uses interpreter fallback
    set(JIT_BACKEND_SRC src/jit_arm32_stub.c)
    set(JIT_EMIT_SRC)
else()
    set(JIT_EMIT_SRC src/jit_emit.c)
    set(JIT_BACKEND_SRC src/jit_x86_64.c)
endif()

add_executable(jvm
    src/jvm.c
    src/main.c
    ${JIT_BACKEND_SRC}
    ${JIT_EMIT_SRC}
)
target_include_directories(jvm PRIVATE include)
set_source_files_properties(src/jvm.c src/main.c PROPERTIES LANGUAGE CXX COMPILE_FLAGS "-Wno-deprecated")
if(JIT_BACKEND_SRC)
    set_source_files_properties(${JIT_BACKEND_SRC} ${JIT_EMIT_SRC} PROPERTIES LANGUAGE CXX)
endif()

install(TARGETS jvm DESTINATION ${CMAKE_INSTALL_BINDIR})
