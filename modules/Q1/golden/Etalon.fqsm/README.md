## Etalon.fQSM Header Projection Spec

This folder is a hand-maintained projection reference for turning Q1 DSL into fQSM-facing C++ headers.

The authoritative source is `../Etalon.q1/aspects.q1.types`. The C++ header is only a projection of that model. In this folder, `aspects.q1.h` is the aspect-level target, while `elementary.q1.h` shows the same projection mindset for plain structural types. This document is intentionally header-only and does not yet specify `.cpp` behavior.

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
- `component C of Host` -> `struct C : Component<C, Host>`
- `group<E> of Host` -> a dedicated group aspect type, e.g. `GroupName : Group<GroupName, E, Host>`
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

For Q1 reactions, the argument inside `!name(...)` declares the analysis scope.

This is not an ordinary function parameter list. It is a DSL-level declaration of what kind of changes the reaction analyzes.

### `one` reactions

For reactions declared in the `one` block, the scope is expressed in the language of changes related to one item.

- `!name(-one)` means reaction to deletion of one item
- `!name(=one)` means one-item-local normalization on item change
- `!name(>one)` means one-item-triggered world-aware corrector on item change

The symbol inside `one` reactions therefore carries two pieces of meaning:

- what kind of item event wakes the reaction up
- what kind of worker shape the projection is expected to use

Practical reading:

- `-one` -> deletion-triggered worker
- `=one` -> local per-item corrector
- `>one` -> world-aware per-item corrector

### `all` reactions

For reactions declared in the `all` block, the scope is expressed in the language of types whose changes are analyzed.

- `!name(~)` means aspect-wide analysis with the owning aspect type implicitly included
- `!name(~OtherType)` means aspect-wide analysis where `OtherType` is added as an explicit source type

The owning aspect type is always implicit for `all` reactions. An explicit `~OtherType` does not replace the owning type; it extends the set of types whose changes may wake the reaction up.

Canonical examples from this etalon:

- `!some_logic_fieldwide_invariant(~)` means aspect-wide analysis over the owning type only
- `!limit_by_tag_count(~Tag)` means aspect-wide analysis over the owning type plus `Tag`

Canonical example from `one` reactions:

- `!sync(>one)` means wake up on changed `Remnant` items and compute a corrected `Quantum` with world access

### Why

This distinction is easy to lose if one reads `!name(...)` as if it were just another function signature. It is not. The parentheses describe reaction analysis scope:

- in `one`, scope is phrased as item-level wakeup plus worker shape
- in `all`, scope is phrased as analyzed source types

That difference is part of Q1 semantics and must survive projection.

### Normative rules

- Any aspect with explicit `!` rules needs `struct Internals;`
- Such an aspect also needs `static const Behavior customAspectReactions();`
- The header only declares that custom behavior exists; binding details stay outside the header

### Implied custom behavior

Some Q1 fields imply nontrivial behavior even without an explicit `!` line:

- `anchor<T>` implies custom internal behavior
- `control<T>` implies custom internal behavior

Therefore:

- explicit `!` means custom reaction-bearing aspect
- `anchor<>` or `control<>` also mean custom reaction-bearing aspect
- no `!` and no `anchor<>`/`control<>` means the aspect may stay trivial at header level

### Trivial form

If an aspect is trivial at header level, it may use:

- `struct Internals : DefaultInternals{};`
- `static const Behavior customAspectReactions() { return {}; }`

### Why

`Internals` is not a mirror of syntax alone. It is a mirror of syntax plus semantics implied by special field kinds. This matters for code generation because anchor/control are not ordinary payload fields; they carry behavioral consequences even before `.cpp` projection is described in full.

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
4. Missing data in Q1 must not quietly reappear in C++.

That is the intended discipline for this etalon.
