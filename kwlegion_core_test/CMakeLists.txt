find_package(Qt6 REQUIRED COMPONENTS Core Sql Test)

add_executable(kwlegion_core_tests
    main.cpp
    prospector_tests.cpp
    queries_tests.cpp
    store_tests.cpp
    transaction_tests.cpp
)

target_link_libraries(kwlegion_core_tests
    PRIVATE
        kwlegion::core
        Catch2::Catch2
        Qt6::Test
)

target_compile_definitions(kwlegion_core_tests
    PRIVATE
        REPLAY_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/replays"
)

# queries.h/exception.h are private headers that live alongside queries.cpp
# rather than under kwlegion_core's public include/ directory, so the test
# target needs this path explicitly to exercise Queries directly.
target_include_directories(kwlegion_core_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/kwlegion_core/src
)

add_custom_command(TARGET kwlegion_core_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:kwlegion_core_tests>
        $<TARGET_FILE_DIR:kwlegion_core_tests>
    COMMAND_EXPAND_LISTS
)

# QSqlDatabase loads QSQLITE as a plugin at runtime rather than linking it
# directly, so it needs to be deployed into a sqldrivers/ folder next to the
# test executable - otherwise QSqlDatabase::addDatabase("QSQLITE") silently
# fails to find a driver.
add_custom_command(TARGET kwlegion_core_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:kwlegion_core_tests>/sqldrivers
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:Qt6::QSQLiteDriverPlugin>
        $<TARGET_FILE_DIR:kwlegion_core_tests>/sqldrivers
)

catch_discover_tests(kwlegion_core_tests)
