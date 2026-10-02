# Agent rules

Read this file before editing. This repository is a prototype being rebuilt in stages; its code, SQL, diagrams, and earlier product rules are evidence, not an approved target specification.

## Confirmed direction

- The user wants an architecture-first, staged rebuild, with regular small Conventional Commits. Aim for fewer than 100 changed code lines per commit when practical; keep each commit coherent and tested.
- The product is a cross-platform smart-home client. The launch platforms are open for review; the current Android/Windows Qt app does not settle that choice.
- Reconsider both the UI framework and backend. Do not migrate merely for novelty: compare options against confirmed platform, device, operations, cost, and security needs.
- The actual devices, gateway, protocol, telemetry writer, and telemetry contract are not known. Do not invent hardware integrations, database columns, tables, or writer credentials.

## Architecture workflow

- Separate observed behavior, user-confirmed requirements, and proposals in documentation.
- Before choosing a database model or implementing device control, identify how real devices receive commands and report state. Inspect the real telemetry producer when available.
- Prove the selected path with a simulator and one representative real integration before expanding the app.
- Prefer end-to-end vertical slices with explicit pending, acknowledgement, timeout, failure, and reconnect behavior.
- Preserve the existing application until its replacement slice is validated. Remove legacy code only when its replacement and migration implications are understood.

## Security and quality

- Never embed service-role credentials or device secrets in a client build. Never log passwords, access/refresh tokens, or secret keys. Keep TLS verification enabled.
- If Qt/QML remains, use C++20 without extensions, fix `-Wall -Wextra -Wpedantic`, keep HTTP/parsing/token handling out of QML, use `qsTr` for visible strings, and set accessible names on interactive controls.
- Add focused unit, contract, and integration tests at the boundaries being changed. Do not treat a passing UI/database write as proof that a physical device acted.
- Preserve unrelated work. Do not commit secrets or generated build output.

## Current implementation map

The current Qt application is built with CMake. `qml/` contains screens, `src/app/` contains the HTTP/session code, `src/core/` contains rules and JSON parsing, `tests/` contains core, API-client, and session-controller tests, and `supabase/migrations/` contains a provisional schema. These paths describe the prototype, not a permanent architecture.

The current desktop baseline uses Qt 6.11.2 MinGW at `C:\Qt\6.11.2\mingw_64` and `C:\Qt\Tools\mingw1310_64`. Keep platform-specific setup documented only after the target platforms are chosen.
