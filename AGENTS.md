# Agent rules

Read this file before editing. The old QML prototype is not the spec.

## Product

Qt 6.11 client for Android and Windows. A person signs in and sees rooms and devices stored in one Supabase project. Lights and plugs are switched from this app. Sensor readings are written by another repository that sends telemetry to the same cloud. This app reads those readings. It does not invent temperatures.

Weather for one saved city comes from Open-Meteo.

The other repository's name, path, and table shape are unknown. Do not invent them. Do not add a telemetry table in this repo until that shape is known.

`SessionController::walkStaleReadings` writes a fake temperature. That is leftover. Do not call it from new code, and do not delete it until the other repository's columns are known.

## Out of scope

Scenes, schedules, a notification inbox, GPS, a Worker, Clerk, Docker, and Postgres that we host. Do not put the Supabase URL or keys in the UI.

## Code

- C++20, extensions off. Fix `-Wall -Wextra -Wpedantic` in our files.
- QML calls the controller. Parsing, token refresh, and HTTP stay in C++.
- User-visible strings use `qsTr`. Pages use layouts. Controls set `Accessible.name`.
- Load `SmartHome/Main` with `loadFromModule`. Do not set a custom QML resource prefix.
- Name REST columns. Check a UUID before putting it in a URL. Send the desired on/off value.
- One refresh at a time, when the access token expires within 60 seconds. Transfer timeout is 15 seconds. Leave TLS verification on.
- Do not log the access token, refresh token, anon key, or password.
- The service role key is never a build variable. URL and anon key come from gitignored `config.local.cmake`.

## Limits

- Password 8–72 characters. Names 1–40. City empty or 1–80. Email at most 254.
- Greeting bands are 05:00–11:59, 12:00–16:59, 17:00–20:59, and 21:00–04:59.
- Poll every 20 seconds only while the application is active. Send nothing while it is hidden.
- Access token lifetime is the Supabase default of 1 hour. Sign-in lasts until logout. There is no 5-in-15 lockout.
- Colors are `#18171C`, `#2F2F37`, `#536DED`, and `#FFFFFF`. Name text is 32px. Labels are 14px. Secondary text is 11px.

## API

Auth: `POST /auth/v1/signup`, `POST /auth/v1/token?grant_type=password`, `POST /auth/v1/token?grant_type=refresh_token`, `POST /auth/v1/logout`.

Data: `profiles`, `rooms`, `devices` through PostgREST. Send `apikey` and `Authorization: Bearer`. Writes send `Prefer: return=representation`.

Weather, with no Supabase call:

- `https://geocoding-api.open-meteo.com/v1/search?name={city}&count=1&language=en&format=json`
- `https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}&current=temperature_2m,weather_code,is_day`

Schema: `supabase/migrations/0001_home.sql`. Paste it in the SQL editor. The CLI is not required. Do not rename that file.

## Layout

`qml/` screens, `src/app/` HTTP and session, `src/core/` rules and JSON, `tests/` for those core pieces, `supabase/migrations/` for the schema. Diagrams are in `docs/architecture.md`. Run steps are in `README.md`.

## Definition of done

- `appSmartHome` builds with the desktop Qt 6.11.2 MinGW kit.
- `ctest` passes for the tests that the machine is allowed to start, and `appSmartHome_qmllint` reports 0 warnings.
- A new screen or request matches this file. No new device kind, table, or vendor.
- No project URL, anon key, service role key, access token, or refresh token is committed.
- User-visible strings use `qsTr`. Interactive controls set `Accessible.name`.
- One Conventional Commit on `smarthome-app`. Do not push.

## Checks

Desktop kit: `C:\Qt\6.11.2\mingw_64`. Compiler: `C:\Qt\Tools\mingw1310_64`. Android package id is `org.mutuma.smarthome`. A new account has 0 rooms until the person adds one.
