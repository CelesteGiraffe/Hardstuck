# Plugin code AGENTS guidelines

These instructions cover all code under `/workspace/Hardstuck/Hardstuck`, including headers, implementations, resources, and supporting scripts unless superseded by a deeper `AGENTS.md`.

## Architectural expectations
- Keep responsibilities separated: backend/API calls in `backend/` and `src/backend/`, payload composition in `payload/` and `src/payload/`, history tracking in `history/`, settings in `settings/`, UI overlays in `ui/` and `src/ui/`.
- Favor small, composable classes and functions; avoid mixing UI, settings, and backend logic.
- Maintain the existing pattern of headers in the root subfolder and implementations mirrored under `src/`.

## C++ style and safety
- Target C++17; avoid compiler-specific extensions unless already present.
- Enforce RAII and clear ownership: prefer values, then `std::unique_ptr`; use `std::shared_ptr` only for intentional shared ownership.
- No raw `new`/`delete` in high-level logic; wrap allocations in smart pointers or values.
- Maintain const-correctness and pass large objects by `const&`.
- Avoid blocking work (file I/O, network, JSON) on game or render threads; schedule long operations via background tasks.
- Validate pointers and external data from BakkesMod APIs before use.

## Error handling and logging
- Prefer explicit return types (`bool`, `std::optional`, small structs) for recoverable failures.
- Centralize logs through existing helpers (e.g., `DiagnosticLogger`) and keep them actionable, not spammy.
- Do not swallow exceptions silently; provide context when catching.

## File layout and naming
- Match existing naming: types `PascalCase`, methods `PascalCase`/`camelCase` (stay consistent with nearby code), member fields with trailing `_` or `m_` prefix as present.
- Keep functions under ~40 lines when reasonable; extract helpers to preserve readability for late-night maintainers.
- Avoid `using namespace std;` in headers.

## Testing and diagnostics
- Prefer unit-size validation where practical (payload formatting, JSON serialization) rather than game-dependent tests.
- When adding diagnostics, ensure they can be disabled or throttled via cvars.

## UI-specific cautions
- The in-game overlay is currently incomplete; avoid expanding it unless necessary. If you must touch UI, keep it behind cvars and ensure it fails safely when ImGui context is absent.
