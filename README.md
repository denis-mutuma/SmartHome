# SmartHome

Qt 6.11 client for Android and Windows. Accounts and rooms live in one Supabase project. Weather comes from Open-Meteo.

Read `AGENTS.md` and `docs/design.md` before changing the app.

## Run the desktop app

1. Create a Supabase Free project and turn off email confirmation.
2. Run `supabase/migrations/0001_home.sql` in the SQL editor.
3. Copy `config.example.cmake` to `config.local.cmake` and fill in the project URL and anon key.
4. Configure with `C:\Qt\6.11.2\mingw_64` and build `appSmartHome`.

`config.local.cmake` stays off git. The service role key stays out of the build.

## Checks

From the desktop build directory, run `ctest` and the `appSmartHome_qmllint` target.
