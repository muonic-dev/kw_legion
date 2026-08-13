find_package(Qt6 REQUIRED COMPONENTS Core Sql)

add_library(kwlegion_core STATIC
    # These sources trigger moc for the builddid I 
    include/kwlegion_core/prospector.h
    include/kwlegion_core/store.h
    include/kwlegion_core/transaction.h
    src/prospector.cpp
    src/store.cpp
    src/transaction.cpp
    src/queries.cpp
)
add_library(kwlegion::core ALIAS kwlegion_core)

set_target_properties(kwlegion_core PROPERTIES AUTOMOC ON)

target_include_directories(kwlegion_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(kwlegion_core
    PUBLIC
        Qt6::Core
        Qt6::Sql
        legionparser::legionparser
)
