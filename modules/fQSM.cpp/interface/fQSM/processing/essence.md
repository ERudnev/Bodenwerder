# Processing: how a change becomes state

## Contexts

- `Reading` (View) gives read access to a state: a Realm, or a Future over a Realm.
- `Writing` (Gate) gives read access plus buffered writes. Writes go into a patch, not into the state.
- `Stewarding` (Dock) gives direct access (`direct<X>().items`) to the Realm lines plus a Gate. Direct writes change the Realm in place and mark the aspect as tainted.
- `Reacting` (Review) is the context of a reaction: `changes<X>()` reads the patch under review, `adjustments<X>()` writes into a new correction patch.
- `Retrospecting` (Wall) reads the last stable state; deletion reactions use it.

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

A Branch owns a patch over a base view. It does not normalize. When the Branch closes, its patch is merged into the patch of the parent (another Branch, or the Realm). The Realm normalizes and integrates the result.

## Principles

- Objects carry state. Knowledge of valid state is in the rules and reactions, not in the objects.
- Changes are more important than events.
- The Realm state is the primary source of information.
- Reactions operate on intended changes, not on changes that already occurred.
