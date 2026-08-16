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
        qml/CNCKW_Marked_of_Kane_logo.png
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

install(TARGETS kw_legion
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
