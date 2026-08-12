find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(legionparser_tests
    legionparser_tests.cpp
)

target_link_libraries(legionparser_tests
    PRIVATE
        legionparser::legionparser_static
        Catch2::Catch2WithMain
)

target_compile_definitions(legionparser_tests
    PRIVATE
        REPLAY_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/replays"
)

add_custom_command(TARGET legionparser_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:legionparser_tests>
        $<TARGET_FILE_DIR:legionparser_tests>
    COMMAND_EXPAND_LISTS
)

catch_discover_tests(legionparser_tests)
