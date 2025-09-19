cmake_minimum_required(VERSION 3.16)

project(QTweaker VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appQTweaker
    main.cxx
)

qt_add_qml_module(appQTweaker
    URI QTweaker
    VERSION 1.0
    QML_FILES Main.qml
    QML_FILES QML/TopBarButton.qml
    QML_FILES QML/SideMenuRadioButton.qml

    RESOURCES appresources.qrc
    SOURCES precompiled.hxx
    SOURCES qmetacompat.cxx
    SOURCES qmetacompat.hxx
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appQTweaker PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appQTweaker
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_precompile_headers(appQTweaker PRIVATE precompiled.hxx)

target_link_libraries(appQTweaker
    PRIVATE Qt6::Quick
    PRIVATE Core
)

include(GNUInstallDirs)
install(TARGETS appQTweaker
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
add_subdirectory(NativeImage)
add_subdirectory(Core)
add_subdirectory(Backend)
