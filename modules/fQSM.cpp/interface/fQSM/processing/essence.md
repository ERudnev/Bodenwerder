# Features — Current Model

## Core Entities

### Codex

A **codex** defines which world changes are of interest.

A codex contains a set of **norms**.

### Norms

A **norm** defines which world state is considered valid.

A norm analyzes **Preview** and produces a reaction patch.

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

**View** + the patch under consideration + **Preview** of the future state.

Used by reactions and norms.

## Integration

```
A
│
├─ P  (logic patch)
│   ▼
│   Review(A + P)
│   │
│   ├─ Reactions / Norms
│   │   ▼
│   │   V  (reaction patch)
│   │
│   ▼
│   Integrate(P)
│       ▼
│       B
│           ▼
│           Integrate(V)
```

- Reactions do **not** modify the patch under consideration.
- Reactions produce their **own** patch.

## Principles

- Objects are **carriers of state**.
- Knowledge of state validity lives **outside** objects.
- Causality is **directed**.
- **Changes** matter more than events.
- World state is the **primary source of information**.
- Reactions operate on **intentions of change**, not on facts that have already occurred.
- The codex is applied **recursively** until a consistent state is reached.
