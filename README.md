# SmartHome

Qt 6.11 / QML / C++ prototype for Android and Windows. Accounts and rooms currently use Supabase; weather uses Open-Meteo. Device switches update database rows and thermometer readings are simulated, not hardware telemetry.

The real-device product direction and undecided integration choices are explained in [docs/architecture.md](docs/architecture.md).

Coding agents use [AGENTS.md](AGENTS.md); explanatory documents are references, not additional instructions.

## Run the desktop app

1. Create a Supabase Free project and turn off email confirmation.
2. Run every numbered SQL migration in `supabase/migrations/` in order in the SQL editor. Resolve any preflight findings before continuing.
3. Copy `config.example.cmake` to `config.local.cmake` and fill in the project URL and anon key.
4. Configure and build with the desktop presets below; the current Windows baseline is Qt 6.11.2 / MinGW 13.1.

`config.local.cmake` stays off git. The service role key stays out of the build.

## Desktop Presets

[CMakePresets.json](CMakePresets.json) uses Ninja and separate `build/desktop-debug` (tests enabled) and `build/desktop-app` (app only) directories. Configure the chosen preset before building it; keep kit/compiler paths local.

```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.2\mingw_64\bin;$env:PATH"
cmake --preset desktop-debug -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/mingw_64 -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe
cmake --build --preset desktop-debug
ctest --preset desktop-debug
cmake --build --preset desktop-debug --target appSmartHome_qmllint
```

Use `desktop-app` for configure/build without tests. Machine-specific settings can also live in ignored `CMakeUserPresets.json`: use uniquely named presets inheriting the shared presets, and point matching local build/test presets at the local configure preset. Do not store credentials in either preset file.

## Build the Android app

Install the Qt 6.11.2 Android arm64 kit and configure its matching Android SDK and NDK in Qt Creator. Use the same Supabase configuration described above.

Build the `appSmartHome_make_apk` target from the Android build directory. The APK is written under `android-build-appSmartHome/build/outputs/apk/` and uses package ID `org.mutuma.smarthome`. `apk_all` also packages the Android Qt Test APKs. CMake project version `1.0.0` supplies the runtime, bundle, Android version name, and derived version code.

## Checks

`BUILD_TESTING` defaults to `ON`, enabling the five test executables and requiring Qt Test. Configure with `-DBUILD_TESTING=OFF` to build without test targets or the Qt Test dependency; `appSmartHome_qmllint` remains available.

From the desktop build directory, run `ctest` and the `appSmartHome_qmllint` target.
