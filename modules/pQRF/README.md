# pQRF

**persistent Quantized Relational Form** — backends for fQSM aspects that declare a retrospection form via `Meta::describe` (`one`/`all` × `field`/`collection`). The aspect-side vocabulary lives in fQSM; this module maps that form onto storage.

- `database/` — SQLite (separate tables for quanta, collections, globals).
- `json/` — standard JSON archive: positional rows, no field names; `Id` as `"0x…"`; collections inlined in the same quantum/global row. Per-aspect object keys: `one`, `one.collections`, `all`, `all.collections` (collection *data* is inlined inside `one`/`all` rows; the `*.collections` keys are reserved labels matching the four relational roles).

Authorship: designed and implemented by Composer 2.5 (Cursor agent / LLM), in collaboration with the DAQL project maintainers.
