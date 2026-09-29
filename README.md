# SmartHome

Qt 6.11 client for Android and Windows. Accounts and rooms live in one Supabase project. Weather comes from Open-Meteo.

Read `AGENTS.md` and `docs/design.md` before changing the app.

## Run the desktop app

1. Create a Supabase Free project and turn off email confirmation.
2. Paste `supabase/migrations/0001_home.sql` in the SQL editor. The `0001_` prefix orders that one file by hand. The Supabase CLI names files with a timestamp; this project does not use the CLI.
3. Copy `config.example.cmake` to `config.local.cmake` and fill in the project URL and anon key. After you edit it, run CMake again. The URL and anon key are compile definitions.

`config.local.cmake` stays off git. The service role key stays out of the build.

### Qt Creator

Open `CMakeLists.txt`. Kit: Desktop Qt 6.11.2 MinGW 64-bit. Run target: `appSmartHome` (not `tst_*`). Creator adds Qt and MinGW to `PATH`. Android kit: Qt 6.11.2 for Android ARM64-v8a, same target, package `org.mutuma.smarthome`.

### VS Code

Install CMake Tools and the C/C++ extension. Generator: Ninja. `CMAKE_PREFIX_PATH`: `C:/Qt/6.11.2/mingw_64`. Compiler: `C:/Qt/Tools/mingw1310_64/bin/g++.exe`. Build `appSmartHome`. `PATH` must start with `C:\Qt\6.11.2\mingw_64\bin` and `C:\Qt\Tools\mingw1310_64\bin`, or the process exits before a window.

## Checks

From the desktop build directory, run `ctest` and the `appSmartHome_qmllint` target.
