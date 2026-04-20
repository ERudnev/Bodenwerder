# Transactions Contracts

This folder contains transaction kinds that share one common transport contract and differ by policy.

## Shared Model

`Transaction` has three different responsibilities:

- read view:
  `operator Reading()`, `operator->()`, `operator*()`
- input policy:
  `absorb(Commit::Result)`
- final output policy:
  `on_finish()`

`Commit::Result` is the payload passed between parent and child transactions:

- `delta` is the accumulated change packet
- `maybeState` is optional and is sent only by transaction kinds that consider their current `head.state` a meaningful result

Rule:

- sender decides whether its `state` is meaningful and exposes that through `maybeState`
- receiver decides what to do with received `maybeState` and `delta`

## Meaning Of `absorb`

`absorb(...)` defines how a transaction kind accepts the result of a nested transaction.

Examples:

- `Accumulator` ignores `maybeState` and only accumulates `delta`
- `Sequence` accepts `maybeState` when present, otherwise integrates `delta`
- `Branch` accepts the result, then validates it

## Meaning Of `on_finish`

`on_finish()` defines the final result that this transaction sends to its parent.

Examples:

- `Accumulator` returns only accumulated `delta`
- `Sequence` returns both `head.state` and accumulated `delta`
- `Branch` returns both `head.state` and `delta(root -> head)`

## Transaction Kinds

### `Accumulator`

- read view does not advance
- collects only `delta`
- ignores incoming `maybeState`
- returns `{ {}, delta }`

Use when local state materialization is not the point of the transaction.

### `Once`

- single-shot delta forwarder
- no public read/write conversions
- ignores incoming `maybeState`
- returns only `delta`

Use for submit-only helpers such as atomic create/declare paths.

### `Staged`

- batches typed add/remove/update operations
- keeps baseline read view
- ignores incoming `maybeState`
- returns only `delta`

### `Sequence`

- continuously materializes local `head.state`
- accumulates final `delta` in parallel
- if child sent `maybeState`, accepts it as the new `head.state`
- otherwise applies `integrate(head.state, delta)`
- returns both `head.state` and accumulated `delta`
- does not validate on `absorb(...)`

This is the main "stateful but non-validating" transaction kind.

### `Branch`

- owns a meaningful evolving world state
- on input:
  accepts `maybeState` when present, otherwise integrates `delta`
- after input acceptance:
  runs `validate_smart(...)`
- on output:
  returns both `head.state` and `delta(root -> head)`

`Branch::cleanChannel()` is the internal non-validating receive path used for nested validation work.

### `Quantum`

- single-item modifier transaction
- emits one atomic delta if dirty
- does not return `state`

## Helper Transactions Outside This Folder

Some helper transactions/modifiers live outside this directory but follow the same contract.

Current example:

- `helpers/global.h::modifier` returns only `delta`, not `state`

## Design Intent

Heavy transaction kinds that truly build a meaningful current world may return `maybeState`.

Lightweight helper/modifier transactions should normally return only `delta`.

The contract is intentionally asymmetric:

- sender knows whether its state is meaningful
- receiver knows how to consume that result
