cmake_minimum_required(VERSION 3.21)

project(EduSync VERSION 1.0.0 LANGUAGES CXX)

# C++ setup
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Nice Qt defaults (automoc/uic/rcc, warnings, etc.)
include(CheckIPOSupported)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# Find Qt 6 with WebEngine
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    WebEngineWidgets
)

# Qt helper sets AUTOMOC/AUTOUIC/AUTORCC and warnings sensibly
qt_standard_project_setup()

# Sources
set(SRC
    src/main.cpp
    src/Event.cpp
    src/SuperAI.cpp
    src/UltraMainWindow.cpp
    src/ModernCalendarWidget.cpp
    src/WeekHeaderView.cpp
    src/UltraDashboardRender.cpp
)

set(HDR
    src/Event.h
    src/SuperAI.h
    src/UltraMainWindow.h
    src/ModernCalendarWidget.h
    src/WeekHeaderView.h
    src/UltraDashboardRender.h
)

# Application target (Qt6 style). MANUAL_FINALIZATION lets us call qt_finalize_executable().
qt_add_executable(EduSync
    MANUAL_FINALIZATION
    ${SRC}
    ${HDR}
)

# Link Qt libs
target_link_libraries(EduSync PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
)

# macOS bundle niceties (safe to keep on all platforms)
set_target_properties(EduSync PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# On Linux runners, ensure RPATH points to the local Qt if needed (keeps artifacts runnable)
if(UNIX AND NOT APPLE)
  set_target_properties(EduSync PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
  )
endif()

# Finalize (generates proper app bundle/Info.plist on macOS, resources, etc.)
qt_finalize_executable(EduSync)
