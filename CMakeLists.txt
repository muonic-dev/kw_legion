cmake_minimum_required(VERSION 3.23)

project(kw_legion
    VERSION "0.3.0"
    DESCRIPTION "A C&C 3: Kane's Wrath replay manager"
    LANGUAGES CXX)

# CMake's project(VERSION) accepts only numeric components, so keep it numeric
# and derive a SemVer pre-release label separately for packaging/display.
set(KW_LEGION_VERSION_PRERELEASE "snapshot")
if (KW_LEGION_VERSION_PRERELEASE STREQUAL "") 
set(KW_LEGION_VERSION_SEMVER "${PROJECT_VERSION}")
else()
set(KW_LEGION_VERSION_SEMVER "${PROJECT_VERSION}-${KW_LEGION_VERSION_PRERELEASE}")
endif()

# Short commit hash for the About page's build field, kept separate from
# KW_LEGION_VERSION_SEMVER so a rebuild of the same release tag is still
# distinguishable. This only refreshes when CMake reconfigures, not on every
# rebuild - acceptable for a value that's purely informational.
find_package(Git QUIET)
set(KW_LEGION_GIT_HASH "unknown")
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short=8 HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE KW_LEGION_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE KW_LEGION_GIT_HASH_RESULT
    )
    if(NOT KW_LEGION_GIT_HASH_RESULT EQUAL 0 OR KW_LEGION_GIT_HASH STREQUAL "")
        set(KW_LEGION_GIT_HASH "unknown")
    else()
        execute_process(
            COMMAND ${GIT_EXECUTABLE} status --porcelain
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE KW_LEGION_GIT_DIRTY
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT KW_LEGION_GIT_DIRTY STREQUAL "")
            string(APPEND KW_LEGION_GIT_HASH "-dirty")
        endif()
    endif()
endif()

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Any subdirectory that calls qt_add_qml_module() (kwlegion_core, kwlegion_ui)
# needs this to have run first, so it lives here rather than in one of them.
find_package(Qt6 REQUIRED COMPONENTS Core)
qt_standard_project_setup(REQUIRES 6.10)

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

add_subdirectory(legionparser)
add_subdirectory(kwlegion_core)
add_subdirectory(kwlegion_ui)

include(CTest)

if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
    )
    FetchContent_MakeAvailable(Catch2)

    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)

    add_subdirectory(legionparser_test)
    add_subdirectory(kwlegion_core_test)
endif()

install(FILES
    ${CMAKE_SOURCE_DIR}/LICENSE
    ${CMAKE_SOURCE_DIR}/README.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}
)

set(CPACK_PACKAGE_NAME "KW Legion")
set(CPACK_PACKAGE_VENDOR "KW Legion")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A C&C 3: Kane's Wrath replay manager")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "kw-legion-${KW_LEGION_VERSION_SEMVER}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "KW Legion")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# Without this, CPack's NSIS generator installs the files but skips creating
# any Start Menu shortcut - it only builds one per entry listed here.
set(CPACK_PACKAGE_EXECUTABLES "kw_legion" "KW Legion")

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "KW Legion")
    set(CPACK_CREATE_DESKTOP_LINKS "kw_legion")
    # Adds a "Run KW Legion" checkbox (checked by default) to the finish
    # page.
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "kw_legion")
    # kw_legion.exe now carries this same icon as a native Win32 resource
    # (see kwlegion_ui/cpp/kw_legion.rc.in) - this covers the installer.exe
    # itself and its uninstaller entry in Add/Remove Programs.
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/kwlegion_ui/ico/kw_legion.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/kwlegion_ui/ico/kw_legion.ico")

    # If kw_legion is already running, ask it to quit over its
    # SingleInstanceGuard local-socket pipe (see
    # kwlegion_ui/cpp/singleinstanceguard.cpp) before overwriting its files.
    # The app defaults to minimizing to tray on close, so window state can't
    # tell us it actually exited - instead poll whether its exe file is
    # still locked, which is what actually blocks the install.
    # The actual script lives in packaging/nsis/ rather than being embedded
    # directly as this variable's value: CPack's own CPackConfig.cmake
    # generator copies CPACK_* string values verbatim into a new
    # set(VAR "value") with no re-escaping at all, so anything embedded here
    # has to already be safe for that one-way trip:
    #  - No literal double quotes at all - confirmed they corrupt regardless
    #    of source (a two-argument NSIS command needing quotes, and even a
    #    single quoted !include path, both got mangled). NSIS's !include
    #    doesn't require quotes for a path with no spaces, so this is a
    #    quote-free bare path rather than a quoted one.
    #  - Backslashes doubled, not left single: !include didn't resolve the
    #    forward-slash form of this path, so it's a native backslash path -
    #    but a single backslash here would let a segment like "\nsis" get
    #    silently misread as CMake's own \n (newline) escape when cpack
    #    re-parses the generated file. Doubling collapses back to single
    #    backslashes on that re-parse instead.
    set(_kw_legion_preinstall_path
        "${CMAKE_SOURCE_DIR}/packaging/nsis/quit_and_wait_preinstall.nsh")
    string(REPLACE "/" "\\\\" _kw_legion_preinstall_path
                   "${_kw_legion_preinstall_path}")
    set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS
        "!include ${_kw_legion_preinstall_path}")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
