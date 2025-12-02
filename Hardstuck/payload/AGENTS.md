# Payload formatting AGENTS guidelines

Scope: `/workspace/Hardstuck/Hardstuck/payload` and `/workspace/Hardstuck/Hardstuck/src/payload`.

- Keep payload builders deterministic: given the same inputs, they should produce the same JSON/body without hidden state.
- When adding fields, document source data and expected types in comments near the builder functions.
- Prefer small structs or `std::optional` for intermediate data instead of magic values.
- Centralize string keys and playlist IDs as named constants to avoid drift across payloads.
- Add lightweight validation helpers for input structs rather than embedding checks deep inside serialization.
