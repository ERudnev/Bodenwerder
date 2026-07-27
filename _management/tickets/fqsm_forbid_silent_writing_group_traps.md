# Ticket: запретить «тихие» ловушки Writing / Group / QuantumGate

**Источник:** `stories/TomSawyer` invaders — рестарт после game over.

**Цель тикета:** не «починить invaders», а **изменить fQSM так, чтобы такой код было трудно или невозможно написать** (compile-time / assert / API shape), и зафиксировать канон в HOWTO.

---

## CRITICAL — чинить сегодня (после проверки, что мелочь не ушла)

**Статус:** поймано через `RealmSafe` + `finish_patch` (не throw из `~Gate`).  
**Симптом сейчас:** `Engine error: Bad optional access` (`std::bad_optional_access`), на **втором** R после lost (первый рестарт может пройти).  
**Приоритет:** **CRITICAL уровня fQSM. Фикс сегодня**, когда убедимся, что параллельно не уплыла рыбешка помельче (Group stale / invaders API). Пока invaders **не** «чиним ради билда» — сначала библиотека.

### Механика (факт по коду)

`Patchlet = std::optional<T>`; deletion = ключ с `nullopt`.

```cpp
// base/cannonball/patch.h — modify_modification
if (auto* patchlet = Base::find(id))
    return patchlet->value(); // deletion → bad_optional_access
```

`QuantumGate::open_patchlet` (`quantal.h`): если patchlet есть, но `!has_value()` (уже delete), early-return **не** срабатывает → зовёт `update_modification` → `.value()` на пустом optional.

**Инвариант, который библиотека обязана держать:** в одном patch нельзя тихо превратить «remove → modify того же id» в `bad_optional_access`. Нужен явный контракт (refuse / assert / семантика resurrect / запрет modify после delete) и тест.

Файлы:

- `modules/base.cpp/interface/base/cannonball/patch.h` — `modify_modification`
- `modules/fQSM.cpp/interface/fQSM/processing/orchestrators/quantal.h` — `QuantumGate::open_patchlet`
- отладка: `RealmSafe::finish_patch`, брейк на `modify_modification` при `patchlet && !has_value()`

---

## Симптом (исторический стек, до RealmSafe)

Краш на разрушении `Writing` конца кадра:

```
~Operational / ~shared_ptr<Operational>
~Gate
Engine::beginFrame
Application::run
```

После `RealmSafe` та же логика всплывает на `finish_patch` исключением, а не в деструкторе.

---

## Ловушка 1 — фабрика второго контекста из `Reacting` (**истреблено в воркспейсе**)

Именованный `Writing` из `Reacting` в реакциях убран (raidenmamare, TomSawyer, Q1 golden, entity_relations, `Functional::action`). Канон в HOWTO: реакция держит `Reacting`, дальше тот же `context`.

Остаётся: при желании сделать конверсию на границе `with<>` `explicit` / резать линтером.

---

## Ловушка 2 — вложенные `QuantumGate` на один Id

**Уточнение после разбора:** `modify` из реакции штатно пишет в следующий patch (adjustments). Сам nested modify на один id в одном K — не «кривой patch»; отдельно от CRITICAL выше.

### Как писали

```cpp
auto session = with<Session>::modify(context, session_id); // gate жив
...
Bootstrap::resetMatch(context, session_id); // внутри снова Session::modify
```

### Что сделать в библиотеке

- При необходимости: registry «уже открыт Id» → явный fail, не тихий хаос.
- Scope gate = локальные поля; не держать через тяжёлые вызовы без нужды.

---

## Ловушка 3 — сырой `remove` worker’а мимо Group

### Как писали

```cpp
void clearAliens(Writing context, Session::Id session) {
    for (const auto id : aliens) {
        destroyVisual(context, with<Alien>::get(context, id).visual); // Node::remove
        with<Alien>::remove(context, id); // мимо Alien_group
    }
}
```

Спрайты создаются так:

```cpp
with<Node_group>::addElement(context, root, ...);
with<Alien_group>::addElement(context, session, Alien::Quantum{...});
```

Правильный API группы:

```cpp
with<Alien_group>::deleteElement(context, session, alien_id);
// или
with<Alien_group>::clear(context, session);
```

`deleteElement` / `clear` синхронно **erase Id из Fat Quantum хоста** и снимают worker. Сырой `Client::remove` **не** чистит set на Group — в кванте остаются мёртвые Id. Structural сейчас чистит элементы при **удалении Group**, но не вычищает членство при удалении Element.

То же для `Node`: `destroyVisual` → `Node::remove` без `Node_group::deleteElement(root, node)`.

### Что сделать в библиотеке

- Либо structural reaction: Element removed → erase id из всех Group\<…, Element\> hosts (дорого, но безопасно).
- Либо **запретить** доменный `with<Element>::remove`, если Element является worker’ом Group: только `Group::deleteElement` / `clear` (category constraint / deleted BaseActions::remove для grouped workers).
- Тест: addElement × N → raw remove всех → group quantum пуст (сейчас падает / оставляет мусор) → после фикса — либо compile error на raw remove, либо group пуст.

---

## После отладки — истребить (остаток)

Отладочный `RealmSafe` + ручные `finish_patch` в `Application` временно держат мир Томми. **Пока ловим CRITICAL — не снимать.**

Когда разбор CRITICAL закончен:

- Вернуть Томми на обычный `Realm` (или оставить `RealmSafe` как явный debug-инструмент fQSM).
- Убрать временные `finish_patch` из wrapper’а, если отладочный режим больше не нужен.

**Сделано (подтаска):** `assets::Manager::prepare` принимает `Writing`, не `Realm`/`RealmSafe&`. pQRF README: `loadFromLocation(Stewarding, …)`.

---

## Критерий готовности тикета

0. **CRITICAL:** `remove` → `modify`/`update_modification` того же id в одном patch не даёт `bad_optional_access`; явный контракт + тест. **Фикс сегодня** после проверки, что мелочь не ушла.
1. ~~Именованный `Writing` из `Reacting` в реакциях~~ — вычищен по воркспейсу + HOWTO (только канон с `context`). Дальше: при желании `explicit`/lint на конверсию у `with<>`.
2. Повторный `modify` того же Id при живом outer `QuantumGate` — явный fail (assert/refuse), если решим что это запрещено.
3. Worker Group нельзя «тихо» снять сырым `remove` без согласованного erase из host set (API или structural).
4. HOWTO_entity_relations + короткий anti-pattern блок со ссылкой на этот тикет и на invaders-рестарт как repro.
5. ~~Места «ссылка на Realm вместо контекста записи»~~ — `prepare(Writing)`; не тащить оркестратор в assets API.

---

## Repro (прикладной, не обязательно в CI)

`TomSawyer` + `RealmSafe` → Enter → lost → R (иногда ок) → R снова → `Bad optional access` на `finish_patch`.

Файлы-улики:

- `stories/TomSawyer/model/tommy/invaders/session.cpp` — `attractAndRestart` + `resetMatch`
- `stories/TomSawyer/model/tommy/invaders/bootstrap.cpp` — `clearAliens` / `clearShots`
- `stories/TomSawyer/model/tommy/invaders/visual.h` — `destroyVisual` → `Node::remove`
- `modules/fQSM.cpp/interface/fQSM/aspect/actions.h` — `Group::addElement` / `deleteElement` / `clear`
- `modules/fQSM.cpp/interface/fQSM/processing/orchestrators/quantal.h` — `QuantumGate`
- `modules/base.cpp/interface/base/cannonball/patch.h` — `modify_modification`
- `modules/fQSM.cpp/.../realm_safe.*` — отладочный оркестратор
