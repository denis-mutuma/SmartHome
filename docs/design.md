# Design

Decisions and screens use STAR. A Result without a number is incomplete. The API table is below the screens.

## Decisions

### Services instead of a Worker

- Situation: Azure, Google, and AWS trial credits are gone, and an idle server still costs money.
- Task: Keep accounts and the home online without hosting an API.
- Action: One Supabase Free project for Auth and Postgres. Open-Meteo for weather. No Clerk and no Worker.
- Result: 0 TypeScript files, 0 Worker requests, 1 Supabase project, $0 while the free quotas hold.

### Supabase Free quotas

- Situation: The household is one account on a phone and a PC.
- Task: Stay inside a published free tier.
- Action: Use the Free plan and PostgREST. Do not add a second vendor for auth.
- Result: $0, 50,000 monthly active users, 500 MB database, unlimited API requests, 5 GB egress. A quiet project pauses after 7 days. Pro, from $25 a month, does not pause.

### Clerk is not used

- Situation: Clerk was the first auth service considered.
- Task: Sign in from Qt and store rooms and devices.
- Action: Do not integrate Clerk.
- Result: 0 Qt SDK from Clerk. Hobby sessions are fixed at 7 days. Home rows would still need another database.

### C++20

- Situation: Qt 6.11.2 requires C++17 and documents C++20 for application code. It has no C++23 page.
- Task: Pick the newest standard Qt documents for this kit.
- Action: `CMAKE_CXX_STANDARD` 20, required, extensions off. Warnings `-Wall -Wextra -Wpedantic` on our files.
- Result: The project standard is 20. MinGW 13.1 at `C:\Qt\Tools\mingw1310_64` is the desktop compiler.

### QML module

- Situation: The prototype loads a `qrc` URL and places widgets at absolute coordinates.
- Task: Follow Qt 6.11's module rules.
- Action: `qt_standard_project_setup(REQUIRES 6.11)`, `loadFromModule("SmartHome", "Main")`, no custom resource prefix.
- Result: 1 module URI, `SmartHome`. `appSmartHome_qmllint` must report 0 warnings.

### Requests

- Situation: The access token lasts 1 hour and the refresh token rotates.
- Task: Keep the session without storing the access token on disk.
- Action: Keep the access token in memory. Refresh is single-flight when expiry is inside 60 seconds or an authorized request returns 401; retry that request once after refresh. Transport, HTTP 408, 429, and 5xx refresh failures preserve the session and queued request for retry on the 15-second timer. Definitive refresh rejection or a malformed successful response clears the session and pending requests. Transfer timeout is 15 seconds. TLS peer checks stay on.
- Result: 15-second timeout, 1 refresh in flight, refresh inside a 60-second window, at most 1 retry per authorized request. Sign-in lasts until logout or definitive refresh rejection because a session timebox is a Pro feature.

### Thermometer

- Situation: Supabase does not simulate a sensor.
- Task: Show a temperature that changes without a cron job.
- Action: The client writes a new value when `reading_at` is null or at least 15 minutes old, clamped to 18.0–28.0. A thermometer's Celsius value is non-null; a new thermometer starts at 22.0.
- Result: 0 Edge Function calls. An idle account writes 0 thermometer rows. The value stays inside an 10-degree span.

### Poll and weather

- Situation: A hidden window should not spend the free egress quota.
- Task: Keep the open screen current and show weather for one city.
- Action: Reload on open, after a successful write, and every 20 seconds while `Qt.application.state` is `Qt.ApplicationActive`. Weather uses Open-Meteo from the device.
- Result: 20-second poll while active, 0 data requests while hidden, 0 Supabase calls for weather, 2 Open-Meteo URLs, 1 city string.

### Email confirmation

- Situation: The built-in mailer allows 2 emails an hour.
- Task: Let a person create an account without waiting for mail.
- Action: Turn confirmation off in the Supabase Auth settings.
- Result: 0 confirmation emails per signup. Signup returns 1 session.

## Screens

### Account

- Situation: The person has no session.
- Task: Create an account or enter an existing one.
- Action: Register asks for first name, email, and password. Login asks for email and password. Logout clears the refresh token.
- Result: 3 registration fields. Password length 8–72. Names 1–40. Email at most 254 characters.

### Home

- Situation: The person is signed in.
- Task: See the home and add the first room.
- Action: Show the greeting plus `first_name`, the weather line, and the rooms. An empty home shows one Add room action.
- Result: 0 rooms on a new account. Greeting uses 4 time bands. Rooms use 1 column below 700px and 2 columns at 700px or wider.

### Room and device

- Situation: A room is open.
- Task: See devices and change a light or plug.
- Action: One tap toggles and sends the desired on/off value. A thermometer shows degrees and has no switch. Edit and delete are secondary. Deleting a room deletes its devices.
- Result: 3 device kinds. 1 tap per toggle. A failed toggle restores the previous value and shows 1 error string.

Colors are `#18171C`, `#2F2F37`, `#536DED`, and `#FFFFFF`. Name text is 32px. Labels are 14px. Secondary text is 11px.

## API

| Call | Body or filter |
| --- | --- |
| `POST /auth/v1/signup` | `email`, `password`, `data.first_name` |
| `POST /auth/v1/token?grant_type=password` | `email`, `password` |
| `POST /auth/v1/token?grant_type=refresh_token` | `refresh_token` |
| `POST /auth/v1/logout` | Bearer access token |
| `GET/PATCH /rest/v1/profiles` | `first_name`, `city` |
| `GET/POST/PATCH/DELETE /rest/v1/rooms` | `id`, `name`, `position` |
| `GET/POST/PATCH/DELETE /rest/v1/devices` | `id`, `room_id`, `name`, `kind`, `is_on`, `celsius`, `reading_at`, `position` |

Every call sends `apikey` and `Content-Type: application/json`. Data calls also send `Authorization: Bearer`. Writes send `Prefer: return=representation`.

Weather:

- `https://geocoding-api.open-meteo.com/v1/search?name={city}&count=1&language=en&format=json`
- `https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}&current=temperature_2m,weather_code,is_day`

Schema migrations live in `supabase/migrations/`. Apply each numbered SQL migration in order in the Supabase SQL editor, resolving any preflight findings first. Desktop and release builds read the project URL and anon key from `config.local.cmake`. Both values are empty in `config.example.cmake`.
