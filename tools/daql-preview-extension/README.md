# KQM Preview

Read-only preview for **KQM** files (`*.tome`).

## Open preview (Cursor)

The tab-corner preview icon from Markdown is a **built-in VS Code feature**. Local extensions in Cursor usually do not get the same button. Use one of these instead:

1. **`Ctrl+K V`** — preview to the side (with a `.tome` file active)
2. **Command Palette** → `Open KQM Preview to the Side`
3. **Right-click** in the editor → `Open KQM Preview`
4. Tab title area → **`...`** or right-click tab → **Reopen Editor With…** → **KQM Preview**

Buttons like **Review Changes** / **Next file** are from **Cursor** (agent/diff review), not this extension.

## Install

```bash
cd tools/daql-preview-extension
npm install
npm run compile
```

**Developer: Install Extension from Location...** → this folder → **Reload Window**.

## Develop

```bash
npm run compile
npm run smoke
```
