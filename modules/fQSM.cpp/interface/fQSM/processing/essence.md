# Features — Current Model

## Core Entities

### Codex

A **codex** defines which world changes are of interest.

A codex contains a set of **norms**.

### Norms

A **norm** defines which world state is considered valid.

A norm analyzes **Draft** and produces a reaction patch.

A norm is a special case of a **reaction**.

### Reactions

A **reaction** analyzes world changes and may produce its own patch.

Reactions may be used for:

- normalization;
- logging;
- diagnostics;
- index maintenance;
- link building;
- any other side effects.

### Reflexes

A **reflex** defines a standard way to respond to a detected situation.

Examples:

- create a missing object;
- remove a violating object;
- register an error;
- reject integration.

The same norm may use different reflexes.

## Contexts

### View

Observable world state.

**Read-only.**

### Writing

**View** + ability to produce a patch.

### Review

**View** + the patch under consideration + **Draft** of the future state + review notes.

Used by reactions and norms.

## Integration

```
A
│
├─ P  (logic patch / current intention)
│   ▼
│   Review(A + P)
│   │
│   ├─ Reactions / Norms
│   │   ▼
│   │   K  (reaction patch)
│   │
│   ├─ if K is empty:
│   │      fixed point reached
│   │
│   └─ else:
│          P <- P ⊕ K
│          repeat Review(A + P)
│
└─ once stable:
       if notes contain rejection:
           abort integration
       else:
           world <- P*
```

- Reactions do **not** modify the patch under consideration.
- Reactions produce their **own** patch.
- Normalization is an iterative patch-to-patch process.
- World integration happens only once, after a stable normalized patch `P*` is found.

## Transactions

### Realm

`Realm` owns the actual world state.

Algorithm:

1. Receive a logic patch `P`.
2. Normalize it recursively into `P*`.
3. If notes contain rejection, do not integrate.
4. Otherwise apply `P*` to the world exactly once.

### Branch

`Branch` does not own world state; it owns only a patch over some base view `A`.

Algorithm:

1. Keep current branch patch `B`.
2. Build `Review(A + B)`.
3. Reactions produce correction patch `K`.
4. Merge: `B <- B ⊕ K`.
5. Repeat until a fixed point is reached or notes reject the branch.

So for a branch the main result is not a new world, but a refined patch.

## Principles

- Objects are **carriers of state**.
- Knowledge of state validity lives **outside** objects.
- Causality is **directed**.
- **Changes** matter more than events.
- World state is the **primary source of information**.
- Reactions operate on **intentions of change**, not on facts that have already occurred.
- The codex is applied **recursively** until a consistent state is reached.
