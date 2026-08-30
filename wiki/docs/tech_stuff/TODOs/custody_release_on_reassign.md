# `custody<T>`: release on reassign still TODO

`custody<T>` is the identity-part pact: holder dies → bury ward; ward murdered → holder dies. Removal only; reverse via `ask::relations`. `custody<T>?` is an orthogonal empty slot (`#?` is a non-owning observer — do not mix).

Still missing: when a live holder **reassigns or clears** the field, bury the previous ward (local `reset`, no encapsulation).

- [ ] Слушать изменение `link` в `reaction::structural::custody`, не только removal
- [ ] Тест на переназначение / `nullopt` при живом `Client`
