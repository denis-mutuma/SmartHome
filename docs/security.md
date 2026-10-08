# Security

This reference describes the prototype's protection and limitations, not a security certification. Integration and account choices remain open; see [architecture](architecture.md).

## Credentials

- Access tokens remain in memory. Refresh tokens use [platform token storage](../src/app/token_store.cpp) and atomic file replacement.
- Windows uses DPAPI for the current OS user. Android uses AES-GCM with a key managed by Android Keystore; hardware backing depends on the device.
- Other platforms refuse non-empty token saves and ignore stored tokens. The controller requires successful persistence before adopting a session, so sign-in is unavailable there until secure storage exists.
- Rejected saves and ignored loads leave existing files untouched. Old plaintext files may still contain credentials; remediation needs explicit owner approval. An empty-token save or explicit clear still removes the session file.
- Android's existing legacy-token migration remains: plaintext is accepted only after it has been successfully replaced with Keystore-encrypted storage. There is no plaintext write fallback.
- Configuration and local presets remain untracked. Never put passwords, refresh/access tokens or privileged backend keys in source, preset files, logs or diagnostics.

## Access and Failure Handling

- The prototype uses Qt's default TLS verification and bounded HTTP transfer timeouts. These protections do not replace endpoint or backend authorization.
- A public backend key is not an authorization boundary. Live access policies need wrong-user and resource-ownership tests before deployment claims.
- Token-persistence failure prevents adopting an incoming session. During refresh it also cancels requests and clears local auth state; discarded requests must not replay into another session.
- A successful database write is not physical device confirmation. Remote access and device credentials require validation against the selected integration.

## Verification Limits

Platform-specific tests do not prove other platforms' storage behavior. Native storage, corruption, persistence failure, logout, revocation and reconnect need testing on each release platform. Do not bypass OS application-control policies to run blocked test binaries; use an approved runner.
