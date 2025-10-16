cmake_minimum_required(VERSION 3.16)

project(EduSync VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt meta-build helpers
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt (add Network only if you actually use it)
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    WebEngineWidgets
    # Network
)

# Sources
set(SOURCES
    src/main.cpp
    src/Event.cpp
    src/SuperAI.cpp
    src/UltraMainWindow.cpp
    src/ModernCalendarWidget.cpp
    src/WeekHeaderView.cpp
    src/UltraDashboardRender.cpp
)

set(HEADERS
    src/Event.h
    src/SuperAI.h
    src/UltraMainWindow.h
    src/ModernCalendarWidget.h
    src/WeekHeaderView.h
    src/UltraDashboardRender.h
)

# App target (macOS bundle)
add_executable(EduSync MACOSX_BUNDLE
    ${SOURCES}
    ${HEADERS}
)

# Link Qt
target_link_libraries(EduSync PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    # Qt6::Network
)

# Nice-to-have bundle flags (already default for MACOSX_BUNDLE, but explicit is fine)
set_target_properties(EduSync PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)


# Required for Qt WebEngine apps (packs resources, etc.)
qt_finalize_executable(EduSync)

