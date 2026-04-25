add_executable(jvlc
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/sema.cpp
    src/codegen.cpp
    src/diag.cpp
)
target_include_directories(jvlc PRIVATE include ${CMAKE_SOURCE_DIR}/jvm/include)

install(TARGETS jvlc DESTINATION ${CMAKE_INSTALL_BINDIR})
