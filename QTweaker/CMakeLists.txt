cmake_minimum_required(VERSION 3.16)

if(WIN32)
    set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
    file(TO_NATIVE_PATH "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" Cmake_toolchain_native)
    set(CMAKE_TOOLCHAIN_FILE "${Cmake_toolchain_native}" CACHE FILEPATH "")
    message("${CMAKE_TOOLCHAIN_FILE}")
    message("${VCPKG_TARGET_TRIPLET}")
endif()

if(DEFINED ENV{QTDIR})
    set(Qt6_DIR "$ENV{QTDIR}/lib/cmake/Qt6" CACHE PATH "Qt6 directory")
    list(PREPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
    message(STATUS "Using Qt from QTDIR: $ENV{QTDIR}")
    message(STATUS "Qt6_DIR set to: ${Qt6_DIR}")
elseif(UNIX AND NOT APPLE)
    list(PREPEND CMAKE_PREFIX_PATH "/usr/lib/cmake")
    message(STATUS "Using system Qt6 on Linux")
endif()

project(tweaker VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(tweaker
    main.cxx

    precompiled.hxx
    qmetacompat.cxx
    qmetacompat.hxx
)

qt_add_qml_module(tweaker
    URI QTweaker
    VERSION 1.0
    QML_FILES Main.qml
    QML_FILES QML/TopBarButton.qml
    QML_FILES QML/SideMenuRadioButton.qml

    RESOURCES appresources.qrc
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(tweaker PROPERTIES

    # MACOSX_BUNDLE_GUI_IDENTIFIER com.example.tweaker
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_precompile_headers(tweaker PRIVATE precompiled.hxx)

add_subdirectory(NativeImage)
add_subdirectory(Core)
add_subdirectory(Backend)

target_link_libraries(tweaker
    PRIVATE Qt6::Quick
    PRIVATE Qt6::QuickControls2
    PRIVATE Core
    PRIVATE Backend
)

include(GNUInstallDirs)
install(TARGETS tweaker
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Автоматическое развёртывание Qt DLL после сборки
if(WIN32)
    # Найти windeployqt
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")

    if(WINDEPLOYQT_EXECUTABLE)
        message(STATUS "Found windeployqt: ${WINDEPLOYQT_EXECUTABLE}")

        # Добавить пост-билд команду для копирования Qt DLL
        add_custom_command(TARGET tweaker POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --qmldir "${CMAKE_SOURCE_DIR}"
                --no-translations
                --no-system-d3d-compiler
                --no-compiler-runtime
                "$<TARGET_FILE:tweaker>"
            COMMENT "Deploying Qt dependencies to output directory..."
        )
    else()
        message(WARNING "windeployqt not found. Qt dependencies will not be automatically deployed.")
    endif()
endif()
