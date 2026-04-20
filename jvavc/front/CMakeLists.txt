add_executable(jvlc
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/sema.cpp
    src/codegen.cpp
)
target_include_directories(jvlc PRIVATE include)
