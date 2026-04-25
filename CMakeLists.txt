cmake_minimum_required(VERSION 3.16)
project(JVAV VERSION 0.2.5 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform & compiler checks --------------------------------------------------
# MSVC is NOT supported (Int128 relies on GCC/Clang extensions or soft emulation).
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(FATAL_ERROR "JVAV requires GCC, Clang, or MinGW-w64. MSVC is not supported.")
endif()

# Enable more warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|MinGW")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|MinGW")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
endif()

# Standard installation directories
include(GNUInstallDirs)

# Static linking for command-line tools (no external DLL/runtime dependency)
if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

# Pass version to source code
add_compile_definitions(JVAV_VERSION="${PROJECT_VERSION}")

# Put all executables in a single directory so tests/integration can find them
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(jvavc/front)
add_subdirectory(jvavc/back)
add_subdirectory(jvavc/tools)
add_subdirectory(jvm)

# Tests -----------------------------------------------------------------------
enable_testing()
add_subdirectory(tests/jvavc/back)
add_subdirectory(tests/jvavc/front)

# Install documentation -------------------------------------------------------
install(FILES README.md README_zh.md LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/docs)
install(DIRECTORY std/ DESTINATION std)

# CPack packaging configuration -----------------------------------------------
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "JVAV Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "JVAV Compiler and JVM")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# CPack archive layout: files are placed under the install prefix

if(WIN32)
    find_program(NSIS_EXECUTABLE makensis)
    if(NSIS_EXECUTABLE)
        set(CPACK_GENERATOR "ZIP;NSIS")
    else()
        set(CPACK_GENERATOR "ZIP")
    endif()
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ;DragNDrop")
else()
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "JVAV Team")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
endif()

include(CPack)
