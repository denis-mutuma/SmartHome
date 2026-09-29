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
- Action: Keep the access token in memory. Refresh is single-flight when expiry is inside 60 seconds, or once after a 401. Transfer timeout is 15 seconds. TLS peer checks stay on.
- Result: 15-second timeout, 1 refresh in flight, refresh inside a 60-second window. Sign-in lasts until logout because a session timebox is a Pro feature.

### Thermometer

- Situation: Supabase does not simulate a sensor.
- Task: Show a temperature that changes without a cron job.
- Action: The client writes a new value only when `reading_at` is older than 15 minutes, clamped to 18.0–28.0. A new thermometer starts at 22.0.
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
