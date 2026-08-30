# Q1 Tooling

This folder contains a small, intentionally narrow toolset for reading and checking the current Q1 golden files.

The tools here are not a production compiler and do not define a new version of Q1. They only formalize the subset that is already used by the current golden inputs:

- `modules/Q1/golden/doctrine/aspects.q1`
- `modules/Q1/golden/doctrine/elementary.q1`

If the language evolves, this folder should be updated by extending the documented assumptions rather than silently changing behavior.

## Contents

- `README.md`
  What is in this folder, how to run it, and current limits.
- `ASSUMPTIONS.md`
  Explicit list of inferred semantics, unresolved ambiguities, and open questions.
- `q1.ebnf`
  Draft EBNF for the subset of Q1 that is actually used by the current golden files.
- `parser.py`
  Small indentation-aware parser that builds a Python AST/IR.
- `linter.py`
  Conservative structural checks on top of `parser.py`.
- `tests/`
  Minimal pytest coverage for the parser and linter.

## Scope

The current tooling is designed for structural reading of the golden files, not for full language implementation.

It understands:

- indentation-scoped namespaces
- `using`, `struct`, `entity`, `attribute`, `feature`, `component`, `group`, `archetype`, `manipulation`
- `struct Name of Base` (single value-struct inheritance)
- aspect blocks `always`, `one`, `all` (block or inline field); `always >name() -> all` Global assembler
- `struct` field categories `always` / `one` / `all` (block or inline)
- operations `?`, `=`, `>`, Stewarding `*name(~Scope[, params...])`, and template ops `?name<P as Constraint>(...)`
- reactions `!name(scope)` with optional effect tail `->>op(...)` / `->=op(...)`
- the currently used type forms such as `#`, `#Type`, `T?`, `anchor<T>`, `custody<T>`, `affects<T>`, `~Type::member`
- a limited amount of `//@` metadata

It does not attempt to implement:

- code generation
- runtime behavior
- semantic execution of reactions
- the full language surface mentioned in `modules/Q1/syntax.txt`

## Running

Examples assume the working directory is this folder or the repository root.

### Parse a Q1 file

```bash
python modules/Q1/tooling/parser.py modules/Q1/golden/doctrine/aspects.q1
```

### Dump JSON

```bash
python modules/Q1/tooling/parser.py modules/Q1/golden/doctrine/aspects.q1 --dump-json
```

### Run the linter

```bash
python modules/Q1/tooling/linter.py modules/Q1/golden/doctrine/aspects.q1
```
### Run tests

```bash
python -m pytest modules/Q1/tooling/tests
```

## Current limitations

- The parser is line-oriented and indentation-aware, not token-complete.
- Only constructs present in the current golden files are formalized.
- Reaction scopes are parsed using the current etalon semantics; they are not treated as ordinary function parameter lists.
- `//@` comments are preserved only in a lightweight way where they help explain declarations.
- Some semantics are inferred from project rules and the golden files rather than from a complete formal language specification.
- Python and pytest are greenfield tooling in this repository; this folder does not try to establish a broader repository-wide Python stack.

## Design stance

- Q1 is the source of truth.
- The tooling should be mechanical and conservative.
- Better to emit a warning than a false hard error.
- Any ambiguity that cannot be resolved from the current golden files and project rules must be written down in `ASSUMPTIONS.md`.
