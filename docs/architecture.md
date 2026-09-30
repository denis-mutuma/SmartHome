# Architecture

This page describes the running client. Rules and limits are in `AGENTS.md`. How to build is in `README.md`.

## Context

The Qt app is the only process in this repo. Supabase holds the account and the home rows. Open-Meteo answers weather. Another repository writes sensor telemetry into the same cloud. Its name and tables are unknown, so this diagram does not invent them.

```mermaid
flowchart LR
  person[Person]
  app[Qt app]
  auth[Supabase Auth]
  db[Postgres via PostgREST]
  weather[Open-Meteo]
  telemetry[Telemetry repo]
  person --> app
  app --> auth
  app --> db
  app --> weather
  telemetry --> db
  app -.->|reads sensor rows| db
```

The access token stays in memory. The refresh token is the only session secret on disk: `session.bin` under the app data directory, sealed with DPAPI on Windows and plaintext in the app-private directory on Android.

## Client

QML draws the screens and calls `SessionController`. The controller does not parse HTTP bodies itself. `ApiClient` performs the requests. `home_json` parses them. `home_rules` checks names, passwords, email, city, and the greeting band.

```mermaid
flowchart TB
  qml[QML screens]
  session[SessionController]
  api[ApiClient]
  json[home_json]
  rules[home_rules]
  tokens[token_store]
  qml --> session
  session --> api
  session --> json
  session --> rules
  session --> tokens
```

`smarthome_core` is the static library of rules, JSON, and the token file, so the tests link those without the GUI. `ApiClient` and `SessionController` belong to the QML module.

## Screens

`Main.qml` keeps one `StackView`. Sign-in replaces the stack with Home. Logout replaces it with Login. The other screens are pushed.

```mermaid
flowchart TD
  login[Login]
  register[Register]
  home[Home]
  settings[Settings]
  room[Room]
  edit[Device editor]
  login --> register
  login --> home
  home --> settings
  home --> room
  room --> edit
```

Home reloads when it opens, after a successful change, and every 20 seconds while the app is active. A hidden app sends no data requests.

## Data

One account owns its profile, rooms, and devices. Deleting a room deletes its devices. A light or plug stores `is_on`. A thermometer stores `celsius` and `reading_at` and has no switch.

```mermaid
erDiagram
  profiles ||--|| rooms : "same account"
  rooms ||--o{ devices : contains
  profiles {
    uuid id
    text first_name
    text city
  }
  rooms {
    uuid id
    uuid user_id
    text name
    int position
  }
  devices {
    uuid id
    uuid room_id
    uuid user_id
    text name
    text kind
    bool is_on
    numeric celsius
    timestamptz reading_at
    int position
  }
```

The picture groups profile and rooms by account. It is not a foreign key from `rooms` to `profiles`. The SQL foreign keys are `profiles.id`, `rooms.user_id`, and `devices.user_id` to `auth.users`, and `devices.room_id` to `rooms`.

`kind` is `light`, `plug`, or `thermometer`. Row level security limits every row to `auth.uid()`.

This app writes rooms, device names, and the desired on/off value. It reads `celsius` and `reading_at`. The telemetry repository is the writer for those two columns. `walkStaleReadings` still writes a fake temperature. That call is leftover and is not part of this architecture.

## Sign-in

Signup and password login both return a session. The client stores the refresh token, then loads the profile and the rooms. A later call refreshes once if the access token expires within 60 seconds, or once after a 401.

```mermaid
sequenceDiagram
  participant QML
  participant Session
  participant Auth as Supabase Auth
  participant Data as PostgREST
  QML->>Session: signIn
  Session->>Auth: password grant
  Auth-->>Session: access and refresh tokens
  Session->>Data: profiles and rooms
  Data-->>Session: rows
  Session-->>QML: home
```

Weather never uses that path. A city change calls Open-Meteo geocoding, then the forecast. A failed forecast keeps the last line from this session.
