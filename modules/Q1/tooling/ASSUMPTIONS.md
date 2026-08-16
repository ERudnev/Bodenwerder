# Q1 Tooling Assumptions

This file lists the places where the current tooling infers semantics heuristically from the existing golden inputs and project rules.

The goal is not to hide uncertainty. The goal is to keep uncertainty explicit.

## Input authority used

The tooling assumes authority only from:

- `modules/Q1/golden/doctrine/aspects.q1`
- `modules/Q1/golden/doctrine/elementary.q1`
- `modules/Q1/syntax.txt`
- `modules/Q1/methodology.tome`
- `modules/Q1/golden/golden/README.md`
- `wiki/docs/Q1/reaction_is_effector.md`

## Explicit assumptions

### 1. `one` / `all` / `always` are the active block vocabulary

`syntax.txt` and `methodology.tome` also mention older names such as `element` / `table` / `static`, but the current golden files use `one` / `all` / `always`.

The tooling therefore treats `one` / `all` / `always` as the active subset and does not parse the older aliases.

### 2. Reaction scopes are not function parameter lists

`!name(...)` is parsed as a reaction declaration with a dedicated scope language, not as a normal call-like signature.

Current supported scopes:

- `-one`, `=one`, `>one` — item-event scopes (`OneScope`)
- `~`, `~Type` — change-source / watch scopes (`AllScope` in AST; usable in both `one` and `all` blocks)

Optional effect tail after the scope `)`:

- `->>name(...)` — yield effector call (`>`): resolves to public **action** if `>name` is declared, else private **reflex** (never invent `>`)
- `->=name(...)` — yield command call (`=`): same resolve rule for public vs Internals

Bare `!name(scope)` remains a whole reaction (no separate tail).

### 3. `>one` remains accepted; object watches use `~` / `~Type` in `one`

Historical `!sync(>one)` is still parseable. Current etalon prefers object reactions in `one` with watch scopes, e.g. `!sync(~Tag)`, `!watch_clock(~Clock)->=field_update()`.

Set-as-set reactions stay in `all` (cardinality / Global / cross-set limits).

### 4. `group<Note> of SampleEntity` is a dedicated aspect form

The parser treats `group<...> of ...` as its own declaration form, not as a generic type expression followed by `of`.

### 5. One-line entity form is accepted only in the currently seen shape

Current golden input uses:

- `entity Note one text: string`

The parser supports this exact shape as a compact `entity` with an inline `one` block and does not try to generalize beyond it.

### 6. `using ... as ~Type::member` is interpreted as member type-of

Used in:

- `using AliasByField as ~Struct::field1`
- `>add_note(#SampleEntity, ~Note::text) -> #Note`

The tooling treats this as a specific type-expression form rather than a general unary operator over arbitrary expressions.

### 7. `@external(...)` is a type expression

Used in:

- `using ExternalDomainType @external(similar to OpenGL texture handle)`
- field types such as `handle: @external(opengl_window)`

The text inside parentheses is a free-form description string. It is not metadata attached to another type; it replaces a type name in that position.

`@cache` and similar field directives remain separate lightweight metadata.

### 8. `@cache` is treated as a field directive only

Used in:

- `power: integer @cache`

The tooling records it as a directive attached to the field and does not assign stronger executable semantics.

### 9. `one<Meta>` is the aspect quantum type projection

Used in archetype operation signatures such as:

- `>createRawNode(#Core, one<Node>) -> #Node`
- `>createCamera(#Core, one<Node>, one<Camera>) -> #Camera`

The tooling treats `one<Meta>` as a dedicated type-expression form meaning “the `one`-block quantum payload of aspect `Meta`”, projected to C++ as `Meta::Quantum`.

### 10. `anchor<T>`, `custody<T>`, and `affects<T>` are recognized as type forms

The parser and linter treat them as special type constructors (AST kinds `AnchorType`, `CustodyType`, `AffectsType`).

Projection notes (C++ / fQSM):

- `anchor<T>` → `Anchor<T>` — structural lifecycle (anchored reaction)
- `custody<T>` → `Custody<T>` — structural lifecycle (custody cleanup)
- `affects<T>` → `Affected<T>` — typed Id link **without** structural lifecycle; lookup via `relation` / `vital`

`anchor<>` / `custody<>` imply custom reaction-bearing aspect behavior. `affects<>` alone does not register a structural reaction; domain `!` rules (and optional `vital`) supply behavior.

### 11. `=` operations may carry a return type

The current golden file contains:

- `=example_op_div_with_remainder(divisor: integer) -> integer`

So the parser allows command operations with an optional return type.

### 12. Parameter name binding qualifiers

Used in named operation parameters:

- `?name: Type` → read-only reference (`binding: "read"`, C++ `const Type& name`)
- `>name: Type` → mutable/out reference (`binding: "mut"`, C++ `Type& name`)
- `name: Type` → unqualified (`binding: null`)

Examples in `golden/doctrine/elementary.q1`:

- `?add_to(>target: StructWithMethods)`
- `=add_from(?source: StructWithMethods)`
- `>build_from(?source: StructWithMethods) -> StructWithMethods`

The prefix is on the **parameter name**, not the type. This is distinct from postfix optional `T?` and from operation-kind prefixes.

### 13. `attribute` aspect declarations

Form:

- `attribute A of Host`

Meaning:

- declares an attribute aspect named `A` parasitic on owner `Host`
- body uses the active block vocabulary `always` / `one` / `all`
- operations and reactions follow the same rules as other aspect bodies in the current golden subset

The parser records `category: "attribute"`.

### 14. `feature` aspect declarations

Form:

- `feature F of Host`

Meaning:

- declares a feature aspect named `F` parasitic on owner `Host`
- body uses the active block vocabulary `always` / `one` / `all`
- operations and reactions follow the same rules as other aspect bodies in the current golden subset

The parser records `category: "feature"`.

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

The string is a logical path relative to the current file's directory, without the `.q1` suffix.

The parser records it as `ImportDecl`. The linter resolves imported modules from sibling `.q1` files and merges their symbols for name checking.

## Known non-goals

- transitive import semantics beyond sibling `.q1` resolution in the linter
- full template *type* declarations (`template` keyword / generic structs) beyond operation template params
- enum parsing
- agent aspect execution semantics
- code generation or runtime validation
- enforcing DSL constraint phrases after `as` (e.g. `feature of Unit`) — recorded only

### Template operations

Operations may carry a type-parameter list after the name:

```
?find<R as feature of Unit>(Unit::Name) -> #R?
```

AST: `QueryOp` / `CommandOp` / `FactoryOp` with optional `template_params: [{kind: TemplateParam, name, constraint}]`.
`constraint` is the text after `as`, or `null` when omitted.
`#R` / `#R?` in the signature are ordinary `IdType` / `OptionalType` nodes whose target is the parameter name.
C++ projection (documented in `q1-to-fQSM.tome` / golden README): `template<::fqsm::meta::category::Any Meta>` and `#R` → `Meta::Id`.

### Builtin container type expressions

Parser and linter support the container forms from `syntax.txt`:

- `vector<T>`
- `set<K>`
- `uset<T>`
- `map<K,V>` (comma-separated key/value type parameters)
- `umap<K,V>`

Type parameters are parsed recursively via `parse_type_expr`, including `#`, `#Aspect`, and nested containers.

In aspect `one`/`all` field bodies, bare `#` is resolved against the enclosing aspect name (local aspect id).

### Nested struct in struct

`struct` bodies may contain nested `struct` declarations (for example `Uniform::Binding`).
Type references `Outer::Inner` resolve when `Inner` is a nested struct or local alias inside `Outer`.

### All-scope `~` in operation parameters

`~` and `~Tag` are valid anonymous parameter markers in query/command/factory signatures (same scope family as reaction `!foo(~)`).
Linter warns with `all-scope-outside-all-block` when `~` appears in a non-`all` block.

### Stewarding operations (`*`)

Prefix `*` declares a Stewarding/Direct hot-pass operation (AST kind `StewardOp`).
The first comma-separated item in `(...)` is a watch-style scope (`~` or `~Type`, AST `AllScope`); it is not a parameter.
Any further items are ordinary params (named in the golden etalon).
Optional `-> type` is allowed by the grammar for symmetry with other ops; golden examples omit return type (void / Stewarding-only).
Preferred placement is `all`; `one` is allowed with the same `~` / `~Type` scope shape. Not part of `always` or value-`struct` subsets.

## Conservative behavior policy

When the tooling cannot prove something from the current golden inputs and rules:

- prefer a warning over a hard error
- keep the raw text where possible
- document the ambiguity here instead of inventing hidden semantics
