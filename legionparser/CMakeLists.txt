find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(legionparser STATIC
    src/parser.cpp
    src/reader.cpp
    src/exception.cpp
)
add_library(legionparser::legionparser ALIAS legionparser)

target_include_directories(legionparser
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(legionparser
    PUBLIC
        Qt6::Core
)
