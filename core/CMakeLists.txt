find_package(Qt6 REQUIRED COMPONENTS Core Sql)

set(KWLEGION_CORE_SOURCES
    include/kwlegion_core/prospector.h
    include/kwlegion_core/store.h
    include/kwlegion_core/transaction.h
    src/prospector.cpp
    src/store.cpp
    src/transaction.cpp
)

add_library(kwlegion_core SHARED ${KWLEGION_CORE_SOURCES})
add_library(kwlegion::core ALIAS kwlegion_core)

set_target_properties(kwlegion_core PROPERTIES AUTOMOC ON)

target_include_directories(kwlegion_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(kwlegion_core
    PUBLIC
        Qt6::Core
        Qt6::Sql
        legionparser::legionparser
)

include(GenerateExportHeader)
generate_export_header(kwlegion_core
    EXPORT_FILE_NAME include/kwlegion_core/kwlegion_core_export.h
)

set_target_properties(kwlegion_core PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Test-only static build. Compiled separately from the sources above rather
# than sharing object files: on Windows, dllexport/dllimport decoration is
# baked into the compiled symbol table, so the same .obj can't correctly
# serve both a DLL build and a static build. This keeps kwlegion_core_tests
# from depending on a runtime-copied DLL going stale.
add_library(kwlegion_core_static STATIC ${KWLEGION_CORE_SOURCES})
add_library(kwlegion::core_static ALIAS kwlegion_core_static)

set_target_properties(kwlegion_core_static PROPERTIES AUTOMOC ON)

target_include_directories(kwlegion_core_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(kwlegion_core_static
    PUBLIC
        Qt6::Core
        Qt6::Sql
        legionparser::legionparser_static
)

target_compile_definitions(kwlegion_core_static PUBLIC KWLEGION_CORE_STATIC_DEFINE)
