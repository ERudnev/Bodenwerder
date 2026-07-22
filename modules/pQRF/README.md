# pQRF

**persistent Quantized Relational Form** — backends for fQSM aspects that declare a retrospection form via `Meta::describe` (`one`/`all` × `field`/`collection`). The aspect-side vocabulary lives in fQSM; this module maps that form onto storage.

- `database/` — SQLite (separate tables for quanta, collections, globals).
- `json/` — standard JSON archive: positional rows, no field names; `Id` as `"0x…"`. `one` = table of quantum rows (collections inlined); `all` = single global row (singleton, not a table of rows). Section keys omitted when empty; missing on load means empty. `present` = aspect object exists in the root. Core API: `capture` / `to_string` / `saveToStream` / `updateFromStream`; file I/O is a thin wrapper.

**Load modes:** `replaceFromLocation(Realm&, …)` fills via Immediate (`Direct`): `items.clear()`, optional `reserve` (SQLite count), `insert` quanta, then global. No delete-then-add patch. `updateFromLocation(Writing, …)` is patch upsert (`restore` if missing, else modify).

## Leaf types (out of the box)

Codecs live in `json/leaf.h` and `database/sql.h`. A `field` / collection element must resolve to one of these (or nest via `field<&…, &member>`).

| C++ / `common_types` | JSON | SQLite |
|---|---|---|
| `bool` / `boolean` | boolean | INTEGER 0/1 |
| `std::int32_t` / `integer` | number | INTEGER |
| `float` | real | REAL |
| `double` / `seconds` | real | REAL |
| `std::string` / `filename` | string | TEXT |
| `std::filesystem::path` / `filepath` | string (generic) | TEXT |
| `timepoint` | number (ms since epoch) | INTEGER (ms) |
| `Identifier<…>` | `"0x…"` string | INTEGER |
| `std::optional<T>` / `base::maybe<T>` | null or T | NULL or T |

Not leaf types: `index2`, `vec2`/`vec3`/`vec4`, `quat`, `mat4`. Flatten with nested `field` on members (e.g. `x`/`y`). Maps/sets are collections, not this table. `mat4` as BLOB is deferred.

`collection<>("name")` with no member pointers: the Quantum (or Global) itself is the container — used for `Group`, where Quantum is `unordered_set<Worker::Id>`.

Authorship: designed and implemented by Composer 2.5 (Cursor agent / LLM), in collaboration with the DAQL project maintainers.
