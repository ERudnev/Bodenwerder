# pQRF

**persistent Quantized Relational Form** — backends for fQSM aspects that declare a retrospection form via `Meta::describe` (`one`/`all` × `field`/`collection`). The aspect-side vocabulary lives in fQSM; this module maps that form onto storage.

- `database/` — SQLite (separate tables for quanta, collections, globals).
- `json/` — standard JSON archive: positional rows, no field names; `Id` as `"0x…"`. `one` = table of quantum rows (collections inlined); `all` = single global row (singleton, not a table of rows). Section keys omitted when empty; missing on load means empty. `present` = aspect object exists in the root. Core API: `capture` / `to_string` / `saveToStream` / `updateFromStream`; file I/O is a thin wrapper.

Authorship: designed and implemented by Composer 2.5 (Cursor agent / LLM), in collaboration with the DAQL project maintainers.
