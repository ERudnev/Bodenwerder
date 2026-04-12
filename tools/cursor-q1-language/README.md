# Q1 Language (Cursor)

Minimal local language extension to get `*.q1.types` highlighted and recognized as **Q1**.

## Syntax notes (DSL)

- Operation **return types** are written with a colon after the closing parenthesis, e.g. `?length(): Scalar`, `?provide(): opengl_window`, `*use(arg: float): float`. (The older `->` form is no longer used in Q1 types.)

## Install in Cursor

1. Open Command Palette.
2. Run **Developer: Install Extension from Location...**
3. Select this folder: `tools/cursor-q1-language`
4. Restart Cursor.

After install, `*.q1.types` should open in Language Mode **Q1** automatically.

