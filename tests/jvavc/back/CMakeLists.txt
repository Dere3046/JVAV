add_executable(test_back
    main_test.cpp
    test_parser.cpp
    test_encoder.cpp
    test_integration.cpp
    ${CMAKE_SOURCE_DIR}/jvavc/back/src/parser.cpp
    ${CMAKE_SOURCE_DIR}/jvavc/back/src/encoder.cpp
    ${CMAKE_SOURCE_DIR}/jvavc/back/src/linker.cpp
)
target_include_directories(test_back PRIVATE ${CMAKE_SOURCE_DIR}/jvavc/back/include ${CMAKE_SOURCE_DIR}/jvm/include)

# Cross-platform executable paths for integration tests
target_compile_definitions(test_back PRIVATE
    JVAVC_BACK_EXE="$<TARGET_FILE:jvavc>"
    JVM_EXE="$<TARGET_FILE:jvm>"
)

add_test(NAME test_back COMMAND test_back)
