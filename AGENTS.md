# Agent rules

Read `docs/design.md` before editing. That file overrides the QML prototype.

## Product

The app is a Qt 6.11 client for Android and Windows. A person signs in, sees rooms, turns a light or plug on or off, and reads a thermometer. Weather comes from Open-Meteo using one saved city.

Do not add scenes, schedules, a notification inbox, GPS, a Worker, Clerk, Docker, or Postgres that we host. Do not put the Supabase URL or keys in the UI.

## Code

- C++20 with extensions off. Fix `-Wall -Wextra -Wpedantic` warnings in our files.
- QML calls the controller. Parsing, token refresh, thermometer math, and HTTP stay in C++.
- QML strings use `qsTr`; C++ UI strings use `tr()` or `QCoreApplication::translate` for free helpers. Pages use layouts. Controls set `Accessible.name`.
- Load `SmartHome/Main` with `loadFromModule`; `AccountMain.qml` is aliased to `Main.qml`. Root `Main.qml` and the unreferenced prototype widgets are legacy sources, not part of the active QML module. Do not set a custom QML resource prefix.
- Name REST columns. Check a UUID before putting it in a URL. Send the desired on/off value.
- Refresh one at a time when expiry is within 60 seconds or an authorized request returns 401; retry each authorized request once after refresh. Transport, 408, 429, and 5xx refresh failures preserve the session and retry on the 15-second refresh timer. Definitive refresh rejection or a malformed successful response clears the session and pending/queued requests so they cannot replay after another sign-in. Transfer timeout is 15 seconds. Leave TLS verification on.
- Do not log the access token, refresh token, anon key, or password.
- The service role key is never a build variable. URL and anon key come from gitignored `config.local.cmake`.

## Commits

Branch `smarthome-app`. Do not push. Conventional Commits: `type(scope): subject`. Types: `docs`, `feat`, `fix`, `test`, `build`, `chore`. Scopes: `docs`, `sql`, `app`, `qml`. One logical change per commit, usually under 100 lines. A decision body is four STAR lines and the Result has a number.

## Limits

- Password 8–72 characters. Names 1–40. City empty or 1–80. Email at most 254.
- Greeting bands are 05:00–11:59, 12:00–16:59, 17:00–20:59, and 21:00–04:59.
- Poll every 20 seconds only while signed in and `Qt.application.state` is `Qt.ApplicationActive`.
- A thermometer write happens when `reading_at` is null or at least 15 minutes old. Keep the value inside 18.0–28.0; thermometer `celsius` is non-null. A new one starts at 22.0.
- Access token lifetime is the Supabase default of 1 hour. Sign-in lasts until logout. There is no 5-in-15 lockout.
- Colors are `#18171C`, `#2F2F37`, `#536DED`, and `#FFFFFF`.

## API

Auth: `POST /auth/v1/signup`, `POST /auth/v1/token?grant_type=password`, `POST /auth/v1/token?grant_type=refresh_token`, `POST /auth/v1/logout`. Data: `profiles`, `rooms`, `devices` through PostgREST. Send `apikey` and `Authorization: Bearer`. Weather uses the two Open-Meteo URLs in `docs/design.md`, not Supabase.

Schema changes live in `supabase/migrations/`. Apply every numbered SQL migration in order in the Supabase SQL editor. Review a migration's preflight queries and resolve reported rows before continuing; the CLI is not required.

## Checks

Desktop kit: `C:\Qt\6.11.2\mingw_64`. Compiler: `C:\Qt\Tools\mingw1310_64`. Run `ctest` and `appSmartHome_qmllint`. A new account has 0 rooms until the person adds one. Android package id is `org.mutuma.smarthome`.
