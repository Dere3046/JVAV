add_executable(disasm
    src/disasm.c
)
target_include_directories(disasm PRIVATE ${CMAKE_SOURCE_DIR}/jvm/include)
