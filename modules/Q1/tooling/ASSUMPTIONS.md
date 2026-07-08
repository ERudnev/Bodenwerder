# Q1 Tooling Assumptions

This file lists the places where the current tooling infers semantics heuristically from the existing golden inputs and project rules.

The goal is not to hide uncertainty. The goal is to keep uncertainty explicit.

## Input authority used

The tooling assumes authority only from:

- `modules/Q1/golden/Etalon.q1/aspects.q1.types`
- `modules/Q1/golden/Etalon.q1/elementary.q1.types`
- `modules/Q1/syntax.txt`
- `modules/Q1/methodology.tome`
- `modules/Q1/golden/Etalon.fqSM/README.md`

## Explicit assumptions

### 1. `one` / `all` / `always` are the active block vocabulary

`syntax.txt` and `methodology.tome` also mention older names such as `element` / `table` / `static`, but the current golden files use `one` / `all` / `always`.

The tooling therefore treats `one` / `all` / `always` as the active subset and does not parse the older aliases.

### 2. Reaction scopes are not function parameter lists

`!name(...)` is parsed as a reaction declaration with a dedicated scope language, not as a normal call-like signature.

Current supported scopes:

- `-one`
- `=one`
- `>one`
- `~`
- `~Type`

### 3. `>one` is accepted as current etalon syntax

The current golden aspect file explicitly contains:

- a commented note that `!sync(>one)` is not implemented as a current Q1 feature
- an active `!sync(~Tag)` replacement

The tooling accepts `>one` syntactically because it is already part of the semantic discussion and documented project rationale, even if it is not currently used as an active rule in the golden file.

### 4. `group<Note> of SampleEntity` is a dedicated aspect form

The parser treats `group<...> of ...` as its own declaration form, not as a generic type expression followed by `of`.

### 5. One-line entity form is accepted only in the currently seen shape

Current golden input uses:

- `entity Note one text: string`

The parser supports this exact shape as a compact `entity` with an inline `one` block and does not try to generalize beyond it.

### 6. `type ... ~ Type::member` is interpreted as member type-of

Used in:

- `type AliasByField ~ Struct::field1`
- `>add_note(#SampleEntity, ~Note::text) -> #Note`

The tooling treats this as a specific type-expression form rather than a general unary operator over arbitrary expressions.

### 7. `@external(...)` is a type expression

Used in:

- `type ExternalDomainType @external(similar to OpenGL texture handle)`
- field types such as `handle: @external(opengl_window)`

The text inside parentheses is a free-form description string. It is not metadata attached to another type; it replaces a type name in that position.

`@cache` and similar field directives remain separate lightweight metadata.

### 8. `@cache` is treated as a field directive only

Used in:

- `power: integer @cache`

The tooling records it as a directive attached to the field and does not assign stronger executable semantics.

### 9. `anchor<T>` and `control<T>` are recognized as type forms

The parser and linter treat them as special type constructors and also record their presence because project rules already interpret them as behavior-carrying field kinds.

### 10. `=` operations may carry a return type

The current golden file contains:

- `=example_op_div_with_remainder(divisor: integer) -> integer`

So the parser allows command operations with an optional return type.

## Open questions

1. Should future tooling also accept the older vocabulary `element` / `table` / `static`, or should that stay outside this tooling folder until a real migration is needed?
2. Should `//@` comments eventually become structured metadata rather than preserved raw text?
3. How far should member validation go for `~Type::member` references in aspect declarations?
4. Should `>one` remain only a documented shape, or should future golden inputs reactivate it as syntax in use?
5. Is the active language subset expected to stay indentation-only, or do future Q1 files plan additional inline compact forms beyond the current one-line entity?

### 11. `import "logical/path"` is a top-level file directive

Used in module aspect files such as:

- `import "window"`
- `import "device"`

The string is a logical path relative to the current file's directory, without the `.q1.types` suffix.

The parser records it as `ImportDecl`. The linter resolves imported modules from sibling `.q1.types` files and merges their symbols for name checking.

## Known non-goals

- transitive import semantics beyond sibling `.q1.types` resolution in the linter
- full template support
- enum parsing
- agent aspect execution semantics
- generic container types beyond what is needed by the current golden files
- code generation or runtime validation

## Conservative behavior policy

When the tooling cannot prove something from the current golden inputs and rules:

- prefer a warning over a hard error
- keep the raw text where possible
- document the ambiguity here instead of inventing hidden semantics
