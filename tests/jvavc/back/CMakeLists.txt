add_executable(test_back
    main_test.cpp
    test_parser.cpp
    test_encoder.cpp
    test_integration.cpp
    ${CMAKE_SOURCE_DIR}/jvavc/back/src/parser.cpp
    ${CMAKE_SOURCE_DIR}/jvavc/back/src/encoder.cpp
    ${CMAKE_SOURCE_DIR}/jvavc/back/src/linker.cpp
)
target_include_directories(test_back PRIVATE ${CMAKE_SOURCE_DIR}/jvavc/back/include)
