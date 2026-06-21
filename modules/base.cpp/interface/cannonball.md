# Cannonball — Current Terms

## Purpose

`cannonball` is a low-level library of dense state containers and change carriers.
It exists to model:

- actual keyed state;
- intended changes to that state;
- projected future world built from `state + patch`.

Names below describe the intended model, not necessarily the current code spelling.

## Core Interfaces

### `table::Read`

Read-only keyed world view.

Responsibilities:

- observe keys and values;
- iterate visible entries;
- know nothing about storage policy.

### `table::Operational`

Read view plus operational updates.

Responsibilities:

- everything from `table::Read`;
- `insert`, `erase`, `clear`, `reserve`;
- mutate the represented world through operations, not through `Val&`.

### `table::Direct`

Operational table with direct mutable value access.

Responsibilities:

- everything from `table::Operational`;
- `Val*` / `Val&` semantics;
- mutable iterator.

This interface is for true owning tables, not for projected proxy worlds.

## Concrete Entities

### `Table`

Owning dense keyed table.

Semantics:

- stores actual values;
- dense storage;
- direct mutable access is allowed;
- suitable as the base implementation for real world state.

### `Patch`

Carrier of intended changes over some keyed world.

Semantics:

- stores only changes, not full state;
- `Patchlet<T> = std::optional<T>`;
- key absent in patch: no local change;
- key present with value: set/update;
- key present with `nullopt`: remove.

`Patch` is a composable change set, not a normalized diff.

### `Preview`

Projected future world for reading only.

Shape:

- `{ const state&, const patch& }`
- behaves as `table::Read`

Purpose:

- show "what the world would look like after this patch";
- hide the fact that it is assembled from state and patch.

### `Draft`

Projected future world for patch building.

Shape:

- `{ const state&, patch& }`
- behaves as `table::Operational`

Purpose:

- give workers a world-like object;
- let them read the future world;
- let them write intentions into the patch as if they were editing the world.

## Naming Notes

`Overlay` is considered a technical implementation idea, not a preferred domain entity name.

Preferred public vocabulary:

- `Table`
- `Patch`
- `Preview`
- `Draft`
- `Delta`

## Design Direction

- world state is primary;
- changes are first-class data;
- projected worlds should look like normal worlds from the outside;
- low-level container mechanics should stay below the domain language.
