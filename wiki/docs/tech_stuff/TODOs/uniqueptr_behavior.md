# `control<T>` как `unique_ptr`

Сейчас `control<T>` убивает зависимую сущность только при удалении владельца. Нужно ещё при **смене id** в поле control — старый `Observed` удалять (как `unique_ptr::reset`).

**Пилот:** `rmmr::Device` — `window: control<Window>`; окно выделить в отдельный аспект `rmmr::Window` (`!release(-one)` там, не на девайсе).

- [ ] Расширить `reaction::structural::controls` в `anchoring.h` (слушать изменение `link`, не только removal клиента)
- [ ] Тест на переназначение control при живом `Client`
- [ ] `device.q1.types` + `window.q1.types` → проекция по Etalon
