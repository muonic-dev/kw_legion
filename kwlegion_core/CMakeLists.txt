find_package(Qt6 REQUIRED COMPONENTS Core Qml Sql)

# Bakes KW_LEGION_VERSION_SEMVER/KW_LEGION_GIT_HASH (set in the root
# CMakeLists.txt) into a header consumed only by appinfo.cpp.
configure_file(
    src/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
    @ONLY
)

set(platform_sources)
if(WIN32)
    list(APPEND platform_sources src/autostart_windows.cpp)
else()
    list(APPEND platform_sources src/autostart_empty.cpp)
endif()

add_library(kwlegion_core STATIC
    # Public interface (and also MOC)
    include/kwlegion_core/appinfo.h
    include/kwlegion_core/prospector.h
    include/kwlegion_core/replaystore.h
    include/kwlegion_core/replaystoremodel.h
    include/kwlegion_core/settings.h
    include/kwlegion_core/ingestionmodel.h
    include/kwlegion_core/inboxitem.h
    # Main src
    src/appinfo.cpp
    src/metatypes.cpp
    src/prospector.cpp
    src/replaystore.cpp
    src/transaction.cpp
    src/queries.cpp
    src/replay.cpp
    src/replaystoremodel.cpp
    src/teammodel.cpp
    src/teammodel.h #MOC
    src/replaymodel.cpp
    src/replaymodel.h # MOC
    src/deferred.cpp
    src/deferred.h #MOC
    src/faction_registration.h
    src/settings.cpp
    src/ingestionmodel.cpp
    ${platform_sources}
    
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
        # Implementation only QML registrations are in src
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        # Generated version.h (see configure_file() above)
        ${CMAKE_CURRENT_BINARY_DIR}
)

# QML_ELEMENT/QML_SINGLETON types (currently just ReplayStoreModel) are registered
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
