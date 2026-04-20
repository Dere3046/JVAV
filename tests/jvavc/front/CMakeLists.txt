add_executable(test_front
    main_test.cpp
    test_lexer.cpp
    test_parser.cpp
    test_sema.cpp
    test_codegen.cpp
    test_integration.cpp
    ${CMAKE_SOURCE_DIR}/jvavc/front/src/lexer.cpp
    ${CMAKE_SOURCE_DIR}/jvavc/front/src/parser.cpp
    ${CMAKE_SOURCE_DIR}/jvavc/front/src/sema.cpp
    ${CMAKE_SOURCE_DIR}/jvavc/front/src/codegen.cpp
)
target_include_directories(test_front PRIVATE ${CMAKE_SOURCE_DIR}/jvavc/front/include)
