# Q1 template operations

First cut: type parameters on aspect operations (`?` / `=` / `>`), not generic `struct`/`entity`.

## Syntax

```
?find<R as feature of Unit>(Unit::Name) -> #R?
```

- `<...>` sits after the operation name, before `(args)`.
- Each parameter: `Name` or `Name as ConstraintPhrase`.
- Constraint phrases are **intent** (recorded by tooling). They are not checked yet.
- `#R` / `#R?` in the signature refer to that type parameter.

See also `modules/Q1/syntax.txt` (template operations) and `modules/Q1/tooling/ASSUMPTIONS.md`.

## C++ / fQSM projection

| Q1 | C++ |
| --- | --- |
| type parameter `R` (aspect meta) | `template<::fqsm::meta::category::Any Meta>` |
| `#R` | `typename Meta::Id` |
| `#R?` | `optional<typename Meta::Id>` |
| `as feature of Unit` (etc.) | comment / future trait check |

`::fqsm::meta::category::Any` requires `Meta::Id` and `Meta::Quantum` (the library concept for aspect metas).

Block rules (`one` → `Reading`+`Id`, `all` → `Reading`, …) still apply as in [golden README](../../modules/Q1/golden/golden/README.md). Shelf components may omit `Id` in hand-written headers when they already do so for sibling ops.

## Precedent

`rmmr::resource::Assets`:

```
?find<R as feature of Unit>(Unit::Name)-> #R?
```

Caller:

```cpp
with<Assets>::find<geometry::Asset>(context, Unit::Actions::name("rmmr", "grid"));
```
