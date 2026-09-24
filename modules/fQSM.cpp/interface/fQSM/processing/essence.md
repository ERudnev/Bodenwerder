# Processing: how a change becomes state

## Contexts

A **session** is one open change: a patch over a base state, and a future that shows the base through the patch. A Realm, a Branch or a normalization wave owns the session. A context is a thin handle to a session. A handle does not own the session. The session outlives every handle.

- `Reading` reads a state: a Realm, a Branch, or a session future.
- `Writing` reads the base plus the patch, and writes into the patch. Copies of a Writing write into one session.
- `Stewarding` is a Writing that also gives `direct<X>()`. `Direct<X>` changes the Realm lines in place and marks the aspect as tainted. The Writing part of the same session sees these changes.
- `Reacting` is the context of a reaction. `changes<X>()` reads the patch under review. `adjustments<X>()` writes into the correction patch of the wave. It converts to Reading (the proposal) and to Writing (the corrections).
- `Retrospecting` reads the last stable state. Deletion reactions get it. Its writes go into the corrections of the wave.
- `SettingUp` gives a Writing to `Always::assemble` while the Realm is built.

Handles count themselves on the session. When the last handle of a Realm session ends, the Realm accepts the session: it normalizes the patch and integrates it. An unnamed Writing ends at the end of the full expression. A named Writing, or a gate from `modify()`, ends at its scope end.

## Runtime shape

- Each aspect has one descriptor, built by `describe<Meta>()` when the aspect is registered. The descriptor holds the value operations of Quantum and Global, the category, the host, the group element and the custom reactions.
- The schema gives each aspect a dense slot. A Realm holds one erased line per slot. Typed code (`with<X>`, `items`, `changes<X>()`) is a thin view that casts at the boundary.
- A transaction holds a patch and a future. Patch lines and future lines are created only for the slots that the transaction touches. The Realm pools these lines and reuses them in the next transaction.

## Normalization (Realm)

```
A: realm state        P: patch of the transaction
repeat (at most 10 waves):
    review A + P
    structural rules of the categories  -> correction patch K
    registered reactions                -> same patch K
    P <- P + K
    stop when K has no changes
if a refusal was recorded: discard P
else: integrate P into A once
```

- A wave visits only the slots whose patch line has changes, or which are tainted by direct access. Taint arms the first wave only.
- Structural rules come from the categories: a parasitic aspect dies with its host, a feature or component takes its host with it, a new parasitic needs its host, a component must appear with its host, a group takes its elements with it, and a removed element leaves every group that holds it. These rules run before the registered reactions in the same wave.
- Reactions do not change the patch under review. They write their own patch.
- A refusal in any wave rejects the whole transaction. Nothing is integrated.

## Branch

A Branch owns a session over the state of its parent (the Realm or another Branch). It does not normalize. When the Branch closes, the parent takes its patch: a Realm normalizes and integrates it, a Branch merges it into its own patch. A refused Branch is discarded alone. Its refusal becomes a warning of the parent Branch, or the rejection of the Realm transaction.

## Principles

- Objects carry state. Knowledge of valid state is in the rules and reactions, not in the objects.
- Changes are more important than events.
- The Realm state is the primary source of information.
- Reactions operate on intended changes, not on changes that already occurred.
