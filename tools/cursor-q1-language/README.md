# Q1 Language (Cursor)

Minimal local language extension to get `*.q1.types` highlighted and recognized as **Q1**.

## Syntax notes (DSL)

- **Imports:** `import "path/to/module"` at the top of a file (see `modules/Q1/syntax.txt`). Paths are logical, without the `.q1.types` suffix; C-style `#include` is not used in Q1 types.
- **Keywords:**
  - `attribute Name of Host` — attribute aspect parasitic on `Host`
  - `feature Name of Host` — feature aspect parasitic on `Host`
- Operation **return types** are written with a colon after the closing parenthesis, e.g. `?length(): Scalar`, `?provide(): opengl_window`, `=use(arg: float): float`. Factories use the `>` marker and must declare a return type, e.g. `>create(...): #` in `element`/`table` (local aspect id: **space between `:` and `#`**), or `>fromScalar(x):StructWithMethods` inside a value `struct`. (The older `->` form is no longer used in Q1 types.)

## Install in Cursor

1. Open Command Palette.
2. Run **Developer: Install Extension from Location...**
3. Select this folder: `tools/cursor-q1-language`
4. Restart Cursor.

After install, `*.q1.types` should open in Language Mode **Q1** automatically.

Operation line prefixes (`?`, `=`, `>`, `!`) use the TextMate scope **`keyword.other.q1.operation-prefix`**. The extension contributes a default **yellow** foreground (`#e2c943`) via `contributes.configurationDefaults` → `editor.tokenColorCustomizations.textMateRules` (global rules object: the scope string only appears in Q1 grammar, so other languages are unaffected).

If the color does not appear: run **Developer: Reload Window**, and confirm the file language mode is **Q1** (not Plain Text). If your user `settings.json` sets `editor.tokenColorCustomizations.textMateRules` to a **full array replacement**, merge this extension’s rule into your array or drop the conflicting key so defaults can apply.

Angle brackets `<` / `>` are not registered as **bracket pairs** for Q1, so a line-leading factory marker `>` is not treated as a stray closing bracket by the editor’s bracket colorization.

### Typing-related scopes (for themes / token inspector)

| Scope | Meaning |
|-------|--------|
| `keyword.other.q1.type-id-hash` | `#` starting an id type (`#`, `#Foo`, part of `#?`) |
| `keyword.other.q1.type-optional-suffix` | Postfix optional `?` in type position (`#?`, `…Entity?`, after `>` / `)` in generics) — not the line-leading query prefix `?name(`, which stays `keyword.other.q1.operation-prefix`. |
