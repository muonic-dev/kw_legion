cmake_minimum_required(VERSION 3.23)

project(kw_legion
    VERSION 0.1
    DESCRIPTION "A C&C 3: Kane's Wrath replay manager"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# clang-tidy/clangd resolve compile_commands.json by walking up from the
# source file to the project root - they don't know to look inside build/.
# Symlinking it to the root works on Linux but needs elevated privileges on
# Windows, so just keep a real copy in sync instead.
add_custom_target(copy-compile-commands ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    BYPRODUCTS ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Copying compile_commands.json to the project root for editor tooling")

add_subdirectory(src)

include(CTest)

add_subdirectory(legionparser)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
