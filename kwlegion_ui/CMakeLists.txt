find_package(Qt6 REQUIRED COMPONENTS Quick LabsPlatform Sql)

qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(kw_legion
    cpp/main.cpp
)

qt_add_qml_module(kw_legion
    URI kw_legion
    QML_FILES
        qml/Main.qml
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
)

install(TARGETS kw_legion
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
