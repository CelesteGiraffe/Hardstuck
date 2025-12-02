# Backend/API AGENTS guidelines

Scope: `/workspace/Hardstuck/Hardstuck/backend` and `/workspace/Hardstuck/Hardstuck/src/backend`.

- Keep HTTP wiring and Rocket League data translation separate; `ApiClient` handles requests, `HsBackend` orchestrates gameplay hooks.
- Avoid blocking the game thread: perform network and file work asynchronously, and clean up finished requests promptly.
- Normalize settings via `SettingsService`; do not hardcode URLs or tokens.
- Validate game state before building payloads; defensive checks should return early with clear logs instead of throwing.
- Prefer small helper methods for payload staging and upload queuing to keep hooks readable.
