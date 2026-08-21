find_package(Qt6 REQUIRED COMPONENTS Quick LabsPlatform Sql)

qt_add_executable(kw_legion
    cpp/main.cpp
)

# qt_add_qml_module() doesn't parse QML_FILES contents for `pragma Singleton`
# - it needs this told explicitly, or the generated qmldir registers it as a
# plain type instead of a singleton, contradicting the file's own pragma.
set_source_files_properties(qml/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(kw_legion
    URI KWLegionUI
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/NavRail.qml
        qml/pages/ReplaysPage.qml
        qml/pages/StatisticsPage.qml
        qml/pages/SettingsPage.qml
        qml/pages/AboutPage.qml
    RESOURCES
        ico/CNCKW_GDI_Logo.png
        ico/CNCKW_Steel_Talons_Logo.png
        ico/CNCKW_ZOCOM_Logo.png
        ico/CNCKW_Nod_Logo.png
        ico/CNCKW_Marked_of_Kane_Logo.png
        ico/CNCKW_Black_Hand_Logo.png
        ico/CNCKW_Scrin_Logo.png
        ico/CNCKW_Traveler-59_Logo.png
        ico/CNCKW_Reaper-17_Logo.png
        ico/question-mark.png
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(kw_legion PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.kw_legion
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(kw_legion
    PRIVATE
    Qt6::Quick
    Qt6::LabsPlatform
    Qt6::Sql
    legionparser::legionparser
    kwlegion::core
    kwlegion::coreplugin
)

# QSqlDatabase loads QSQLITE as a plugin at runtime rather than linking it
# directly, so it needs to be deployed into a sqldrivers/ folder next to the
# dev-build executable - otherwise QSqlDatabase::addDatabase("QSQLITE")
# silently fails to find a driver. This can't be left to piggyback on
# kwlegion_core_test's identical copy (both targets' RUNTIME_OUTPUT_DIRECTORY
# happen to coincide on Windows - see qt_standard_project_setup()) since a
# build that never builds the test target would leave kw_legion.exe without
# it.
add_custom_command(TARGET kw_legion POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:kw_legion>/sqldrivers
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:Qt6::QSQLiteDriverPlugin>
        $<TARGET_FILE_DIR:kw_legion>/sqldrivers
)

install(TARGETS kw_legion
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# install(TARGETS) only knows about the kw_legion binary itself 
# We need the rest of the magic that supports the Qml runtime
# That script only runs as an install-time

qt_generate_deploy_qml_app_script(
    TARGET kw_legion
    OUTPUT_SCRIPT deploy_script
    NO_TRANSLATIONS
)
install(SCRIPT ${deploy_script})
