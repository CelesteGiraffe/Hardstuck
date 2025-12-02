# Repository-wide AGENTS guidelines

These rules apply to the entire Hardstuck repository unless a deeper `AGENTS.md` overrides them. They are written for agents updating the BakkesMod plugin and its supporting materials.

## How to work in this repo
- Read nested `AGENTS.md` files for directory-specific rules before editing files.
- Prefer small, reviewable changes with clear commit messages.
- Keep changes scoped to the plugin code and docs; avoid reformatting unrelated files.
- When touching build scripts or project files, preserve MSVC v143 compatibility noted in `README.md`.

## Testing expectations
- This environment may not build the plugin; note any skipped checks in your summary.
- If you add automated checks, keep them lightweight and documented.

## Documentation and comments
- Document intent, not obvious mechanics.
- Keep instructions discoverable: if you add new subsystems or workflows, update or add an `AGENTS.md` in the relevant folder.

## Performance and safety
- The plugin must remain non-blocking to Rocket League's game thread. Any heavy work needs to be asynchronous.
- Avoid introducing new dependencies unless strictly necessary.

## Style primers
- Follow modern C++17 conventions for code under `Hardstuck/` (see nested agent notes there).
- Use consistent naming and file structure; match the existing header/implementation split.
