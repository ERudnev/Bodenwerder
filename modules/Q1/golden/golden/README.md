## Etalon.fQSM Header Projection Spec

This folder is a hand-maintained projection reference for turning Q1 DSL into fQSM-facing C++ headers.

The authoritative source is `../doctrine/aspects.q1.types`. The C++ header is only a projection of that model. In this folder, `aspects.q1.h` is the aspect-level target, while `elementary.q1.h` shows the same projection mindset for plain structural types. This document is intentionally header-only and does not yet specify `.cpp` behavior.

## Core Principles

### Normative rules

- Q1 is the source of truth.
- The header projection should be as mechanical as possible.
- Public data and public callable API belong in the header.
- Nontrivial reaction wiring may stay out of line.
- C++ must not invent model state that is absent from Q1.
- Q1 comments beginning with `//@` are generator-facing hints and should be preserved unless deliberately replaced by a better hint carrying the same meaning.

### Why

The purpose of this etalon is not to produce "nice C++ by taste". Its purpose is to preserve the Q1 model shape with minimal interpretive drift. The less hidden semantics the projection adds, the easier it is to trust regeneration and to reason about the model from the DSL alone.

## Projection Scope

This spec covers projection from `aspects.q1.types` into `aspects.q1.h`.

It describes:

- aspect category mapping
- nested type layout
- public operation signatures
- criteria for trivial vs custom internals
- header-visible consequences of special Q1 field kinds

It does not yet describe:

- exact `Behavior` composition
- `.cpp` implementation bodies
- component construction helpers
- formal runtime meaning of `@cache`

## Namespace Rule

The Q1 namespace and the C++ namespace must agree on current library identity.

- `namespace Q1_fQSM` in Q1 maps to `namespace Q1_fQSM::...` in C++

### Why

Namespace identity is part of the model surface. The projection should not rename the library behind the user's back.

## Aspect Category Mapping

Each Q1 aspect becomes one C++ `struct` derived from the matching fQSM category:

- `entity X` -> `struct X : Entity<X>`
- `attribute A of Host` -> `struct A : Attribute<A, Host>`
- `feature F of Host` -> `struct F : Attribute<F, Host>`
- `component C of Host` -> `struct C : Component<C, Host>`
- `group<E> of Host` -> a dedicated group aspect type, e.g. `GroupName : Group<GroupName, Host, E>`
- `archetype T` -> `struct T : Archetype<T>`

### Why

The aspect category already carries core semantics. The header should express that directly instead of rebuilding category meaning through ad hoc helper code.

## Block-to-Type Mapping

Q1 aspect blocks map to nested C++ types by role:

- `one` data -> `struct Quantum`
- `all` data -> `struct Global`
- `always` constants and pure helpers -> `struct Always`

### Additional rules

- `Quantum` is the per-item payload.
- `Global` is world-owned aspect-wide state.
- `Global` is emitted only when `all` contains actual data.
- Empty `Global` should be omitted.
- `Always` is for compile-time constants and pure helper functions that do not depend on `Reading`, `Writing`, `Id`, or `Global`.

### Why

This preserves the semantic split already present in Q1:

- item-owned state
- aspect-owned state
- timeless pure/static surface

That split is more important than keeping everything flat in one C++ struct body.

## Public Operation Mapping

Public Q1 operations become declarations in `Actions : BaseActions`, except for pure `always` helpers.

### Mapping table

- `always ?name(...) -> T` -> `static auto name(...) -> T` inside `Always`
- `one ?name(...) -> T` -> `static auto name(Reading, Id, ...) -> T` inside `Actions`
- `one =name(...)` -> `static void name(Writing, Id, ...)` inside `Actions`
- `all ?name(...) -> T` -> `static auto name(Reading, ...) -> T` inside `Actions`
- `all =name(...)` -> `static void name(Writing, ...)` inside `Actions`
- `all >name(...) -> #` -> `static auto name(Writing, ...) -> Id` inside `Actions`
- `all *name(~Scope...)` -> `static void name(Stewarding, ...named params)` inside `Actions` — Direct/Stewarding hot pass; `~` / `~Type` are analysis scope (which aspects to `direct<>`), not C++ arguments
- `?name<R as …>(...) -> #R?` (and `=`/`>` with `<...>`) -> C++ member template on `Actions`:
  `template<::fqsm::meta::category::Any Meta> static auto name(Reading[, Id], ...) -> optional<typename Meta::Id>`
  (`#R` → `Meta::Id`; `as …` is DSL intent only until trait checking exists; `Meta` is any aspect meta with `Id`+`Quantum`)

Note: some shelf `component` surfaces (e.g. `resource::Assets` `one` add/find) historically omit the `Id` argument in hand-written headers — treat that as a local exception, not a change to the table above.

### Parameter name binding qualifiers

Named parameters may carry a prefix on the **parameter name**:

- `?name: Type` -> `const Type& name`
- `>name: Type` -> `Type& name`
- `name: Type` -> unqualified (existing projection rules)

Examples in `elementary.q1.h`:

- `?add_to(>target: StructWithMethods)` -> `void add_to(StructWithMethods& target) const`
- `=add_from(?source: StructWithMethods)` -> `void add_from(const StructWithMethods& source)`
- `>build_from(?source: StructWithMethods) -> StructWithMethods` -> `static StructWithMethods build_from(const StructWithMethods& source)`

### Additional rule

Two DSL operations may keep the same human name if they belong to different blocks and therefore project to different C++ signatures.

Example:

- `always ?from_float(float) -> integer`
- `all >from_float(value_approximate: float) -> #`

This is not a conflict because the projected C++ signatures differ physically through implicit context injection.

### Why

Q1 operation blocks already encode the role of the function. C++ should reflect that role mechanically by adding the corresponding world access context rather than by renaming functions to avoid superficial collisions.

## Validators and Custom Internals

Q1 validators are not projected as public methods.

## Reaction Analysis Scope

For Q1 reactions, the argument inside `!name(...)` declares the analysis scope (not a parameter list).
An optional effect tail may follow: `->>op(...)` or `->=op(...)`.

### Reaction shapes

- whole: `!name(scope)`
- watch + action / reflex: `!name(scope)->>op(...)` — public `>` if declared, else Internals **reflex** (never invent `>`)
- watch + command: `!name(scope)->=op(...)` — same resolve rule for `=`

See `wiki/docs/Q1/reaction_is_effector.md` and `modules/Q1/syntax.txt`.

### `one` reactions (object)

Common case: reaction of an object. Scopes:

- `-one` / `=one` / `>one` — item-event worker shapes (`=one` is strictly local quantum/cache)
- `~` / `~Type` — watch change sources (world / other aspects), still an **object** reaction

### `all` reactions (set as a set)

Rare: cardinality, Global invariants, cross-set limits.

- `!name(~)` / `!name(~OtherType)` — owning type implicit; `~OtherType` extends stimulus set

Canonical set examples:

- `!some_logic_fieldwide_invariant(~)`
- `!limit_by_tag_count(~Tag)`
- `!modulus_clamped(~)` (Global)

Canonical object examples:

- `!min_value(=one)`
- `!sync(~Tag)` on Remnant (in `one`)
- `!watch_trigger(~SampleEntity)->>field_action()` on ReactionSketch

### Why

- in `one`, prefer object semantics (item worker / object watch); fan-out belongs in runtime/codegen, not in hand-written set loops
- in `all`, scope is set-level analysis
- `->>` / `->=` keep mutation behind action / reflex / command and protect public Actions

### Normative rules

- Any aspect with explicit `!` rules needs `struct Internals;`
- Such an aspect also needs `static const Behavior customAspectReactions();`
- The header only declares that custom behavior exists; binding details stay outside the header

### Implied custom behavior

Some Q1 fields imply nontrivial behavior even without an explicit `!` line:

- `anchor<T>` implies custom internal behavior
- `custody<T>` implies custom internal behavior
- `affects<T>` is a typed Id link without structural lifecycle (C++ `Affected<T>`); does not by itself force Internals

Therefore:

- explicit `!` means custom reaction-bearing aspect
- `anchor<>` or `custody<>` also mean custom reaction-bearing aspect
- `affects<>` alone does not; pair with `!` / `vital` as in `Reminder`
- no `!` and no `anchor<>`/`custody<>` means the aspect may stay trivial at header level

### Trivial form

If an aspect is trivial at header level, it may use:

- `struct Internals : DefaultInternals{};`
- `static const Behavior customAspectReactions() { return {}; }`

### Why

`Internals` is not a mirror of syntax alone. It is a mirror of syntax plus semantics implied by special field kinds. This matters for code generation because anchor/custody are not ordinary payload fields; they carry behavioral consequences even before `.cpp` projection is described in full. `affects<>` is the third link form: identity-typed, behavior opt-in.

## Data Honesty Rules

- If data is absent from Q1, it must not silently reappear in C++.
- If Q1 simplified an aspect, the header must tolerate that simplification even if an older projection used to be richer.
- Empty helper structures should not be emitted just for uniformity.
- `@cache` is currently treated as a soft hint to future projection logic, not as a fully formalized header-level construct.

### Why

Projection honesty is more important than preserving legacy richness. A simpler Q1 model should produce a simpler header, even when an older handwritten projection once carried more fields or more machinery.

## Practical Reading Rule

When reading an aspect header in this folder, use this mental model:

1. `Quantum`, `Global`, and `Always` come from Q1 data blocks.
2. `Actions` contains only the public callable API.
3. `Internals` plus `customAspectReactions()` mean that Q1 declared or implied nontrivial behavior.
4. `Internals` in `.cpp` may also hold private helpers when projection needs them — even if (3) does not apply.
5. Missing data in Q1 must not quietly reappear in C++.

That is the intended discipline for this etalon.
