add_executable(jvavc-front
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/sema.cpp
    src/codegen.cpp
)
target_include_directories(jvavc-front PRIVATE include)
