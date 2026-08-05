find_package(Qt6 REQUIRED COMPONENTS Core)

set(LEGIONPARSER_SOURCES
    src/parser.cpp
    src/reader.cpp
    src/exception.cpp
)

add_library(legionparser SHARED ${LEGIONPARSER_SOURCES})
add_library(legionparser::legionparser ALIAS legionparser)

target_include_directories(legionparser
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(legionparser
    PUBLIC
        Qt6::Core
)

include(GenerateExportHeader)
generate_export_header(legionparser
    EXPORT_FILE_NAME include/legionparser/legionparser_export.h
)

set_target_properties(legionparser PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Test-only static build. Compiled separately from the sources above rather
# than sharing object files: on Windows, dllexport/dllimport decoration is
# baked into the compiled symbol table, so the same .obj can't correctly
# serve both a DLL build and a static build. This keeps legionparser_tests
# from depending on a runtime-copied DLL going stale.
add_library(legionparser_static STATIC ${LEGIONPARSER_SOURCES})
add_library(legionparser::legionparser_static ALIAS legionparser_static)

target_include_directories(legionparser_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(legionparser_static
    PUBLIC
        Qt6::Core
)

target_compile_definitions(legionparser_static PUBLIC LEGIONPARSER_STATIC_DEFINE)
