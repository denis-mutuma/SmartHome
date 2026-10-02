# SmartHome

Qt 6.11 prototype for a cross-platform smart-home client. The current build has Android and Windows configurations, but launch platforms and the backend are still under review. The app currently uses Supabase Auth/PostgREST and Open-Meteo; physical device control and the telemetry contract are not implemented or validated.

Read `AGENTS.md` before changing the app. `docs/architecture.md` describes how the pieces connect.

## Build and run the desktop app

The app builds without Supabase configuration and displays a configuration warning at runtime. To exercise the prototype against a Supabase project, copy `config.example.cmake` to `config.local.cmake`, fill in the project URL and anon key, and re-run CMake. These values are compile definitions.

`supabase/migrations/0001_home.sql` is an unvalidated prototype schema. Do not apply it to a non-disposable project or treat it as the approved data model. The external telemetry writer and its contract have not been inspected. The `0001_` prefix orders the prototype file by hand; this repository does not currently use the Supabase CLI.

`config.local.cmake` stays off git. The service role key stays out of the build.

### Qt Creator

Open `CMakeLists.txt`. Kit: Desktop Qt 6.11.2 MinGW 64-bit. Run target: `appSmartHome` (not `tst_*`). Creator adds Qt and MinGW to `PATH`. The current Android configuration uses Qt 6.11.2 for Android ARM64-v8a and package `org.mutuma.smarthome`; this does not settle the launch-platform decision.

### VS Code

Install CMake Tools and the C/C++ extension. Generator: Ninja. `CMAKE_PREFIX_PATH`: `C:/Qt/6.11.2/mingw_64`. Compiler: `C:/Qt/Tools/mingw1310_64/bin/g++.exe`. Build `appSmartHome`. `PATH` must start with `C:\Qt\6.11.2\mingw_64\bin` and `C:\Qt\Tools\mingw1310_64\bin`, or the process exits before a window.

## Checks

From the desktop build directory, run `ctest --output-on-failure` and build the `appSmartHome_qmllint` target. The current suite has five targets: rules, JSON parsing, token storage, API-client, and session-controller tests. Windows Application Control may intermittently prevent a test executable from starting; that is a process-launch restriction, not a test assertion failure.
