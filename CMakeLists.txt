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
    set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS [[
      ; Send Request::Quit (value 1) to the app's local-socket pipe. If
      ; nothing is listening, CreateFileW just fails and we fall through -
      ; the poll below will find the exe already unlocked.
      System::Call 'kernel32::CreateFileW(w "\\.\pipe\kw_legion", i 0x40000000, i 0, i 0, i 3, i 0, i 0) i .r0'
      IntCmp $0 -1 legion_pipe_absent legion_pipe_connected legion_pipe_connected
      legion_pipe_connected:
        System::Call '*(&i1 1) i .r1'
        System::Call 'kernel32::WriteFile(i r0, i r1, i 1, *i .r2, i 0) i .r3'
        System::Free $1
        System::Call 'kernel32::CloseHandle(i r0)'
      legion_pipe_absent:

      StrCpy $2 0
      legion_wait_loop:
        ClearErrors
        Rename "$INSTDIR\kw_legion.exe" "$INSTDIR\kw_legion.exe"
        IfErrors legion_wait_retry legion_wait_done
      legion_wait_retry:
        IntOp $2 $2 + 1
        IntCmp $2 10 legion_wait_prompt legion_wait_sleep legion_wait_prompt
      legion_wait_sleep:
        Sleep 500
        Goto legion_wait_loop

      legion_wait_prompt:
        ClearErrors
        Rename "$INSTDIR\kw_legion.exe" "$INSTDIR\kw_legion.exe"
        IfErrors legion_wait_ask legion_wait_done
      legion_wait_ask:
        MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION \
          "KW Legion is still running and needs to close before installing.$\r$\n\
          Please close it manually, then click Retry." \
          IDRETRY legion_wait_prompt
        Quit

      legion_wait_done:
    ]])
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
