cmake_minimum_required(VERSION 3.16)

project(Backend LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Gui Concurrent Quick QuickControls2)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Concurrent Quick QuickControls2)

add_library(Backend SHARED
  Backend_global.h
  skin_item.hxx
  skin_item.cxx
  precompiled.hxx
  engine.hxx
  engine.cxx
  skin_changer.hxx skin_changer.cxx
  color_configurator.hxx color_configurator.cxx
  tweaker.hxx tweaker.cxx
  wndproc_handler.h wndproc_handler.cpp
  skins_view_model.hxx skins_view_model.cxx
  image_utils.hxx
  image_utils.cxx
  appsettings.hxx appsettings.cxx
  backgrounds_provider.hxx backgrounds_provider.cxx
  icons_provider.hxx icons_provider.cxx
)

target_link_libraries(Backend PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Concurrent Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::QuickControls2)
target_link_libraries(Backend PUBLIC Core)

target_precompile_headers(Backend PRIVATE precompiled.hxx)

target_include_directories(Backend PUBLIC "${CMAKE_CURRENT_LIST_DIR}")

target_compile_definitions(Backend PRIVATE BACKEND_LIBRARY)
