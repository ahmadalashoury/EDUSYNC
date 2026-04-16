# EduSync

EduSync is a macOS desktop calendar built with Qt, C++, and native Apple frameworks. It combines a calmer calendar shell, a futuristic day view, local planning assistance, and one unified account/settings flow instead of a bunch of competing surfaces.

The current app is focused on feeling like one polished product again:
- one main shell
- one settings/accounts flow
- local event storage
- optional external calendar linking
- premium dark and light themes

## What EduSync Does

### Calendar experience
- Month calendar in the left rail with account status and quick context
- Day and week timeline views in the center workspace
- Futuristic event editor for creating and editing blocks
- Refined dark/light visual system across the shell, calendar, and editor
- Dashboard assistant panel for workload, free-time, suggestions, and chat

### Event management
- Create, edit, and delete local events
- All-day events
- Recurrence presets in the editor
- Category-based event labeling
- Persistent local event storage in the app data directory

### Accounts and syncing
- Apple Calendar
- Apple Reminders
- Google Calendar
- Outlook Calendar
- CalDAV accounts including iCloud, Yahoo, Fastmail, and custom servers

### Assistant features
- Local schedule analysis
- Suggestions based on open time and event balance
- Natural-language assistant chat for scheduling and calendar questions
- Optional external LLM key for richer assistant features

## Current Behavior And Limits

This section is intentionally honest.

### Stable and user-facing
- Local events
- Apple Calendar / Apple Reminders linking on macOS
- Google and Outlook account linking
- CalDAV account setup and sync discovery
- Unified settings and account management
- Interactive assistant panel with local insights and optional LLM chat

### Still limited
- Apple and CalDAV outbound create/edit parity is not complete yet
- External provider support should be treated as sync-oriented first, not as full two-way editing for every provider surface
- The app currently targets macOS builds because it links against EventKit and UserNotifications

## Stack

- Qt 6.5+
- C++17
- Objective-C++ for Apple integrations
- CMake
- EventKit
- UserNotifications

## Build Requirements

- macOS
- Qt 6.5 or newer with `Core`, `Gui`, `Widgets`, and `Network`
- CMake 3.16+
- A C++17-capable compiler

## Build

From the repo root:

```bash
cmake -S . -B build
cmake --build build
open build/EduSync.app
```

To run the binary directly:

```bash
./build/EduSync.app/Contents/MacOS/EduSync
```

## Project Structure

- `CMakeLists.txt`: build configuration
- `src/MainWindow.cpp`: main shell, event editor, and app flow
- `src/SettingsDialog.cpp`: unified settings and account center
- `src/CalendarWidget.cpp`: month calendar surface
- `src/DayTimelineWidget.cpp`: day timeline rendering
- `src/WeekTimelineWidget.cpp`: week timeline rendering
- `src/DashboardWidget.cpp`: right-side assistant, metrics, suggestions, and chat
- `src/SyncManager.cpp`: provider lifecycle and sync orchestration
- `src/AppleNativeProvider.mm`: Apple Calendar and Reminders integration
- `src/GoogleCalendarProvider.cpp`: Google OAuth and calendar sync
- `src/OutlookCalendarProvider.cpp`: Outlook OAuth and calendar sync
- `src/CalDAVProvider.cpp`: CalDAV account handling and sync
- `src/LLMAssistantService.cpp`: OpenAI-compatible assistant analysis and chat
- `src/LocalAssistantService.cpp`: local fallback assistant and planning logic
- `src/Theme.h`: visual tokens and global styling

## Privacy Notes

EduSync stores account/session configuration locally through `QSettings` on your Mac. That includes things like:
- Apple access flags
- Google and Outlook tokens
- CalDAV account configuration
- optional assistant API key

Those values are local machine data and are not meant to be committed to Git.

Compile-time credential placeholders live in `src/Credentials.h`. Keep them as placeholders unless you intentionally want to ship your own distribution credentials. Do not commit real secrets there.

## Repository Cleanup Notes

The repo was recently consolidated to remove the older parallel shell/rewrite split. The current direction is:
- preserve the stronger original shell behavior
- keep the useful parts of the rewrite
- remove duplicate flows
- stabilize first
- polish second

## Status

EduSync is now in a much more coherent place than the split-shell state it was in before, but it is still an actively evolving desktop app rather than a finished platform product.
