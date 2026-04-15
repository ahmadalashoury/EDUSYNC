cmake_minimum_required(VERSION 3.16)

project(EduSync VERSION 3.0.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
)

set(SOURCES
    src/main.cpp
    src/Event.cpp
    src/EventStore.cpp
    src/SchedulerEngine.cpp
    src/SyncManager.cpp
    src/MainWindow.cpp
    src/CalendarWidget.cpp
    src/DayTimelineWidget.cpp
    src/WeekTimelineWidget.cpp
    src/SearchPanel.cpp
    src/DashboardWidget.cpp
    src/StatCard.cpp
    src/GoogleCalendarProvider.cpp
    src/OutlookCalendarProvider.cpp
    src/CalDAVProvider.cpp
    src/AppleNativeProvider.mm
    src/NotificationManager.mm
    src/LocalAssistantService.cpp
    src/LLMAssistantService.cpp
    src/SettingsDialog.cpp
)

set(HEADERS
    src/Event.h
    src/EventStore.h
    src/SchedulerEngine.h
    src/CalendarProvider.h
    src/SyncManager.h
    src/Task.h
    src/Habit.h
    src/MainWindow.h
    src/CalendarWidget.h
    src/DayTimelineWidget.h
    src/WeekTimelineWidget.h
    src/SearchPanel.h
    src/NotificationManager.h
    src/DashboardWidget.h
    src/StatCard.h
    src/Theme.h
    src/GoogleCalendarProvider.h
    src/OutlookCalendarProvider.h
    src/CalDAVProvider.h
    src/AppleNativeProvider.h
    src/Credentials.h
    src/AssistantService.h
    src/LocalAssistantService.h
    src/LLMAssistantService.h
    src/SettingsDialog.h
)

add_executable(EduSync MACOSX_BUNDLE
    ${SOURCES}
    ${HEADERS}
)

set_source_files_properties(src/AppleNativeProvider.mm PROPERTIES
    COMPILE_FLAGS "-fobjc-arc"
)

target_link_libraries(EduSync PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    "-framework Foundation"
    "-framework EventKit"
    "-framework UserNotifications"
)

set_source_files_properties(src/NotificationManager.mm PROPERTIES
    COMPILE_FLAGS "-fobjc-arc"
)

set_target_properties(EduSync PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "EduSync"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.edusync.app"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "Copyright © EduSync"
)
