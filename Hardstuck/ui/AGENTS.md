# UI overlay AGENTS guidelines

Scope: `/workspace/Hardstuck/Hardstuck/ui` and `/workspace/Hardstuck/Hardstuck/src/ui`.

- The overlay is experimental; changes should be minimal and behind cvars.
- Do not introduce blocking calls or long-lived allocations in render paths.
- Always guard ImGui usage with context checks (see `Hardstuck::BindImGuiContext`).
- Keep drawing helpers pure and side-effect free; push state in the caller, pop state in the same frame.
- Prefer small, composable widgets over monolithic render functions.
