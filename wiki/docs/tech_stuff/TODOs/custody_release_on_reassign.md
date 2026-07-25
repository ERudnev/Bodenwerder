# `custody<T>`: cleanup on release (reset still TODO)

`custody<T>` is local cleanup duty at a link: when the holder is removed (or, when implemented, when the field Id is reassigned), request deletion of the linked Observed. It is **not** unique_ptr / shared_ptr: no uniqueness, no liveness warranty, stale Ids are valid; external death of Observed must not auto-clear the holder's field.

Сейчас реакция убивает Observed только при удалении владельца. Нужно ещё при **смене id** в поле custody — старый `Observed` удалять (локальный акт release, аналог `reset` без encapsulation).

**Пилот:** `rmmr::Device` — `window: custody<Window>`; окно выделить в отдельный аспект `rmmr::Window` (`!release(-one)` там, не на девайсе).

- [ ] Расширить `reaction::structural::custody` в `anchoring.h` (слушать изменение `link`, не только removal клиента)
- [ ] Тест на переназначение custody при живом `Client`
- [ ] `device.q1.types` + `window.q1.types` → проекция по Etalon
