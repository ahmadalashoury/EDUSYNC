# EduSync

EduSync is a macOS desktop calendar built with Qt, C++, and native Apple frameworks. It combines an editorial premium scheduling shell, day/week planning views, unified account management, and an assistant layer that can work both locally and with an OpenAI-compatible model.

The current app is focused on feeling like one polished product again:
- one main shell
- one settings/accounts flow
- local event storage
- optional external calendar linking
- assistant-guided planning and natural-language scheduling
- premium dark and light themes with an editorial left rail and calmer assistant panel

## What EduSync Does

### Calendar experience
- Left-rail brand header, mini calendar, source visibility, and selected-day event context
- Day and week timeline views in the center workspace
- Premium event editor for creating and editing blocks
- Refined dark/light visual system across the shell, calendar, editor, and assistant rail
- Right-side assistant panel for workload, free-time, suggestions, and chat

### Workspace flow
- Single shell with day/week view switching
- Inline event creation from empty time slots
- Selection-aware edit and delete actions
- Left sidebar source toggles and quick event list for the focused date

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
- Unified account center in Settings instead of separate competing flows

### Assistant features
- Local schedule analysis with no network dependency
- Suggestions based on open time, meeting load, and day balance
- Natural-language assistant chat for scheduling and calendar questions
- Optional OpenAI-compatible endpoint, model, and API key configuration
- Deterministic local fallback when no LLM is configured

## Current Behavior And Limits

This section is intentionally honest.

### Stable and user-facing
- Local events
- Apple Calendar / Apple Reminders linking on macOS
- Google and Outlook account linking
- CalDAV account setup and sync discovery
- Unified settings and account management
- Interactive assistant panel with local insights and optional LLM chat
- Day and week planning views
- Natural-language event creation from the assistant panel

### Still limited
- Apple and CalDAV outbound create/edit parity is not complete yet
- External provider support should be treated as sync-oriented first, not as full two-way editing for every provider surface
- The app currently targets macOS builds because it links against EventKit and UserNotifications
- The assistant is strongest for scheduling and planning tasks rather than broad general-purpose chat

## Stack

- Qt 6.5+
- C++17
- Objective-C++ for Apple integrations
- CMake
- EventKit
- UserNotifications
- OpenAI-compatible chat/completions API for optional LLM assistance

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
- `src/MainWindow.cpp`: main shell, editorial left rail, event editor, and overall app flow
- `src/SettingsDialog.cpp`: unified settings and account center
- `src/CalendarWidget.cpp`: mini month calendar surface and cell rendering
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
- `src/SchedulerEngine.cpp`: deterministic schedule analysis and planning helpers
- `src/Theme.h`: visual tokens and global styling

## Privacy Notes

EduSync stores account/session configuration locally through `QSettings` on your Mac. That includes things like:
- Apple access flags
- Google and Outlook tokens
- CalDAV account configuration
- optional assistant API key

Those values are local machine data and are not meant to be committed to Git.

Compile-time credential placeholders live in `src/Credentials.h`. Keep them as placeholders unless you intentionally want to ship your own distribution credentials. Do not commit real secrets there.

## Status

EduSync is now a coherent desktop calendar product with:
- one main shell
- one account/settings flow
- multi-provider calendar linking
- assistant-backed planning
- premium light and dark modes

It is still an actively evolving desktop app rather than a finished platform product, but the current repo reflects the newer shell, assistant rail, sidebar, and planning experience rather than the older split-app state.
