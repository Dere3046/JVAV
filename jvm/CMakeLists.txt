add_executable(jvm
    src/jvm.c
    src/main.c
)
target_include_directories(jvm PRIVATE include)

install(TARGETS jvm DESTINATION ${CMAKE_INSTALL_BINDIR})
