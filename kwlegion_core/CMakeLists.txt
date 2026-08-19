find_package(Qt6 REQUIRED COMPONENTS Core Qml Sql)

add_library(kwlegion_core STATIC
    # These sources trigger moc
    include/kwlegion_core/prospector.h
    include/kwlegion_core/store.h
    include/kwlegion_core/storemodel.h
    src/teammodel.h
    # Main src
    src/prospector.cpp
    src/store.cpp
    src/transaction.cpp
    src/queries.cpp
    src/replay.cpp
    src/storemodel.cpp
    src/teammodel.cpp
)
add_library(kwlegion::core ALIAS kwlegion_core)

set_target_properties(kwlegion_core PROPERTIES AUTOMOC ON)

target_include_directories(kwlegion_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        # AUTOMOC records QML_ELEMENT headers here by bare filename only
        # (loses the kwlegion_core/ prefix from the PUBLIC include root), so
        # qmltyperegistrar's generated #include guess for the QML type
        # registration TU needs the header's own directory on the path too.
        ${CMAKE_CURRENT_SOURCE_DIR}/include/kwlegion_core
)

# QML_ELEMENT/QML_SINGLETON types (currently just StoreModel) are registered
# under this module, separately from kwlegion_ui's own "kw_legion" module, so
# that kwlegion_core stays a plain, headless, independently testable library -
# nothing here depends on Qt6::Quick or a display.
qt_add_qml_module(kwlegion_core
    URI KWLegionCore
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/KWLegionCore
)

# qt_add_qml_module() generates a separate static-plugin target holding the
# actual type registration entry point. Consumers must link it explicitly -
# linking kwlegion::core alone silently omits the QML registration code.
add_library(kwlegion::coreplugin ALIAS kwlegion_coreplugin)

target_link_libraries(kwlegion_core
    PUBLIC
        Qt6::Core
        Qt6::Qml
        Qt6::Sql
        legionparser::legionparser
)
