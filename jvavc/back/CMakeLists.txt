add_executable(jvavc
    src/main.cpp
    src/parser.cpp
    src/encoder.cpp
    src/linker.cpp
)
target_include_directories(jvavc PRIVATE include)

install(TARGETS jvavc DESTINATION ${CMAKE_INSTALL_BINDIR})
