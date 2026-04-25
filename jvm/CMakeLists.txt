add_executable(jvm
    src/jvm.c
    src/main.c
)
target_include_directories(jvm PRIVATE include)
set_source_files_properties(src/jvm.c src/main.c PROPERTIES LANGUAGE CXX COMPILE_FLAGS "-Wno-deprecated")

install(TARGETS jvm DESTINATION ${CMAKE_INSTALL_BINDIR})
