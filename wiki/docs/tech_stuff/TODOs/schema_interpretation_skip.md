# `schema::aspect` — interpretation не в storage

`archetype` и `manipulation` не заводят линию в `Reality`; в `Graph` для них не должно быть `nodes` / `reactions`.

**Сейчас:** вручную не класть в merge (например `system::Interface` в internal schema Engine).

**Потом:** в `manipulation/schema.h` — `if constexpr` для `Archetype` / `Manipulation` → пустой `Graph`; `merge` уже no-op для пустых частей.
