add_executable(disasm
    src/disasm.c
)
target_include_directories(disasm PRIVATE ${CMAKE_SOURCE_DIR}/jvm/include)
set_source_files_properties(src/disasm.c PROPERTIES LANGUAGE CXX COMPILE_FLAGS "-Wno-deprecated")

install(TARGETS disasm DESTINATION ${CMAKE_INSTALL_BINDIR})
