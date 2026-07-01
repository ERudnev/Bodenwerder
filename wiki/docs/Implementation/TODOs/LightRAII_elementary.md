# Семантика Actions: with\<Meta\>::update

## Чего и зачем

Сейчас элементарное изменение кванта в patch идёт через **`Quantal`** (`ask::item::update<Meta>`): при создании копируется значение из мира в локальный буфер, правки делаются через `operator->`, при разрушении объекта буфер снова записывается в patch. Это микро-RAII на **каждую** правку поля.

Цель — заменить этот паттерн на **прямую мутацию слота в patch** по ссылке `Quantum&`, без промежуточного буфера и без commit в деструкторе. RAII остаётся на уровне **`Writing` / `Branch` / `Realm`** (граница транзакции и интеграция patch), а не на уровне отдельного элемента.

Публичный вызов для домена и тестов — единообразно через **`with<Meta>::update(...)`** (и при необходимости `with<Meta>::change(...)`), по той же оси, что уже приняты `with<>::create`, `with<>::kill`, `with<>::get`.

Итог для автора аспекта:

```cpp
with<SomeComponent>::update(main, id).name = "after";
```

вместо:

```cpp
{
    auto tx = ask::item::update<SomeComponent>(main, id);
    tx->name = "after";
}
```

---

## Проблема текущего `Quantal`

| | `Quantal` (сейчас) | Целевой `update` |
|--|-------------------|------------------|
| Где живёт значение при правке | локальный `buffer` | слот в `patch` |
| Когда попадает в patch | `~Quantal()` | сразу при записи в ref |
| Копии `Quantum` | вход + выход | одна при seed из мира (если ещё нет в patch) |
| Удобство API | guard-объект, нужен scope | `Quantum&`, без лишнего блока |
| Move/copy guard | запрещены | не применимо (ссылка) |

Файлы: `processing/transactions/quantal.h`, alias `manipulation/item.h` → `using update = Quantal<Meta>`.

---

## Целевая семантика (язык Actions)

Параллель уже задана для чтения; дополняем запись:

| | hard | soft |
|--|------|------|
| **read** | `get(Reading, Id)` → `const Quantum&` | `find(Reading, Id)` → `optional<const Quantum&>` |
| **write** | `update(Writing, Id)` → `Quantum&` | `change(Writing, Id)` → `optional<Quantum&>` |
| **create** | `new_element` (Standalone / Parasitic) | — |

Правила **`update` (hard)**:

- Элемент должен существовать в эффективном состоянии (мир + уже наложенный patch); иначе — исключение (как `requireActual` у `Quantal`).
- Если в patch ещё нет записи для `id` — **seed**: копия из мира в patch (`put_add` / `insert`), далее ref на слот patch.
- Если в patch tombstone (`nullopt`) — ошибка (менять удалённое нельзя).
- Мутация через возвращённую ссылку **сразу** меняет patch; отдельного «commit» нет.

Правила **`change` (soft)**:

- Как в экспериментальном черновике `manipulation/item.h` (закомментированный `quantum::change`): если нет в мире и нет живого слота в patch — `nullopt`.
- Tombstone в patch → `nullopt`.

**`remove`**: не через guard `.remove()` на `Quantal`, а отдельная операция (например `with<Meta>::remove` / наследуемый `kill` для parasitic / явный tombstone в patch). Нужно явно спроектировать, чтобы не потерять семантику `Quantal::remove()`.

---

## Где разместить реализацию

- **Объявление**: `aspect/action.h`, mixin `aspect::action::Any<Meta>` — рядом с `get` / `find` / `new_element` (язык среза `Actions`, не глобальный `ask::item`).
- **Диспетчер**: `with<Meta>` = `Meta::Actions` (`manipulation::call_action`), как для `create` / `kill`.
- **Impl**: один проход по patch + world; логика из черновика `quantum::change` / `quantum::get(Writing)` в `manipulation/item.h` (строки 30–70, сейчас закомментировано).
- **`ask::item::update`**: пометить deprecated, оставить alias на `Quantal` на переходный период или убрать после миграции.

Не поднимать в «голый» `ask::` как основной язык домена — только manipulation для низкоуровневых тестов без Actions-обёртки.

---

## План работ (без изменения кода в этом TODO)

### 1. API в `aspect::action::Any`

- [ ] Объявить `static Quantum& update(Writing, Id);`
- [ ] Объявить `static auto change(Writing, Id) -> base::maybe<std::reference_wrapper<Quantum>>;`
- [ ] Реализация: seed patch из мира, tombstone-handling, throw / nullopt по таблице выше
- [ ] Согласовать имена с `PossibleChange` / будущим `model::elementary::Patch` (комментарии в `action.h`)

### 2. Удаление / tombstone

- [ ] Вынести `remove` из микро-RAII: явный API на `Any` или reuse `Parasitic::kill` где уместно
- [ ] Проверить сценарии: `structural_constraints` (`update<EntFree>(...).remove()`), `custom_reactions`, `delta_iterators`

### 3. Миграция вызовов

- [ ] Тесты: `manipulators.cpp` (правка `SomeComponent`), `flat_model_assembly`, `immediate` (`slowJob` / прямые `item::update`)
- [ ] Framework: `features/reactions/constraints.h` (`*item::update<Meta>(...) = *fix`)
- [ ] Внутри `Actions` — unqualified `update` / `change` там, где правка своего или чужого кванта

### 4. Deprecate `Quantal`

- [ ] Пометить `processing/transaction/Quantal` и `item::update` alias
- [ ] Убедиться, что standalone-create через `Quantal(gate, value)` не нужен (заменён `new_element` + `with<>::create`)
- [ ] Удалить после отсутствия ссылок

### 5. Документация и контракт

- [ ] Зафиксировать в контракте слоя `aspect/`: **без префикса** — inherited helpers своего среза; **межаспект и снаружи** — `with<Meta>::...`; **`ask::item::`** — только manipulation-слой (create/exists/update низкого уровня на переход)

---

## Примеры «до / после»

**Тест, правка поля**

```cpp
// было
{
    auto tx = ask::item::update<SomeComponent>(main, id);
    tx->name = "after";
}

// станет
with<SomeComponent>::update(main, id).name = "after";
```

**Реакция / constraint**

```cpp
// было
*manipulation::item::update<Meta>(context, change.id) = *fix;

// станет (вариант)
with<Meta>::update(context, change.id) = *fix;
```

**Мягкая правка**

```cpp
if (auto q = with<Meta>::change(context, id))
    q->field = value;
```

---

## Связанные файлы

| Файл | Роль |
|------|------|
| `interface/fQSM/aspect/action.h` | целевое объявление `update` / `change` |
| `interface/fQSM/manipulation/item.h` | черновик `quantum::change`, текущий alias `update = Quantal` |
| `interface/fQSM/processing/transactions/quantal.h` | текущий микро-RAII (deprecated path) |
| `interface/fQSM/manipulation/_experimental.h` | `with` = `call_action<Meta>` |
| `test/low_level/manipulators.cpp` | эталонный неудобный RAII-фрагмент для замены |

---

## Открытые вопросы

1. **Имя**: только `update` для hard-write или пара `get(Writing)` / `update` как в раннем `quantum::` черновике?
2. **Archetype / `BaseActions` placeholder**: нужен ли `update` на пустом `Archetype::BaseActions` — скорее нет.
3. **Взаимодействие с normalization**: прямая запись в patch не меняет контракт Review — проверить на тестах structural / constraints.
4. **Перенос `with` из `_experimental.h`** в стабильный include — отдельный шаг после стабилизации API.
