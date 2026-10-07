# SmartHome

Qt 6.11 client for Android and Windows. Accounts and rooms live in one Supabase project. Weather comes from Open-Meteo.

Coding agents use [AGENTS.md](AGENTS.md); explanatory documents are references, not additional instructions.

## Run the desktop app

1. Create a Supabase Free project and turn off email confirmation.
2. Run every numbered SQL migration in `supabase/migrations/` in order in the SQL editor. Resolve any preflight findings before continuing.
3. Copy `config.example.cmake` to `config.local.cmake` and fill in the project URL and anon key.
4. Configure with `C:\Qt\6.11.2\mingw_64` and build `appSmartHome`.

`config.local.cmake` stays off git. The service role key stays out of the build.

## Build the Android app

Install the Qt 6.11.2 Android arm64 kit and configure its matching Android SDK and NDK in Qt Creator. Use the same Supabase configuration described above.

Build the `appSmartHome_make_apk` target from the Android build directory. The APK is written under `android-build-appSmartHome/build/outputs/apk/` and uses package ID `org.mutuma.smarthome`. `apk_all` also packages the Android Qt Test APKs. CMake project version `1.0.0` supplies the runtime, bundle, Android version name, and derived version code.

## Checks

From the desktop build directory, run `ctest` and the `appSmartHome_qmllint` target.
