# History tracking AGENTS guidelines

Scope: `/workspace/Hardstuck/Hardstuck/history` and `/workspace/Hardstuck/Hardstuck/src/history`.

- Treat history data as append-only; avoid mutating past records unless migrating formats deliberately.
- Keep JSON schema changes backward-compatible and versioned inside the history types.
- Normalize file access through existing helpers; avoid ad-hoc paths or blocking I/O on the game thread.
- Include lightweight sanity checks (counts, timestamps) before writing history data.
- When adding fields, update serialization and deserialization together and document defaults.
