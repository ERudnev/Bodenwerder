# Три паразита

2026-07-10

До этого в Q1 и fQSM parasitic-аспекты фактически сводились к одной модели: `attribute X of Host`. Для optional decoration этого хватало, но для co-born runtime-связей и mandatory aggregate — нет.

Сегодня в языке появилась отдельная декларация **`feature`**, а в fQSM — отдельная категория **`Feature<Meta, Host>`**. Вместе с уже существующими **`attribute`** и **`component`** получилась лестница из трёх parasitic policy, а не три синонима одного слова.

## Q1

Три параллельные формы (не alias друг друга):

```q1
attribute Tag of SampleEntity
  all
    modulus: integer

feature Window of Driver
  one
    title: string
    size: index2

component Body of Actor
  one
    mass: float
```

Парсер сохраняет keyword как `category: "attribute" | "feature" | "component"`. Проекция в C++ — в `Attribute`, `Feature`, `Component` соответственно.

## Общая база: `Parasitic`

Все три наследуют от `Parasitic<Meta, Host>`:

- shared `Id` с host;
- `remove_with_parent` — при удалении host parasitic удаляется каскадом.

## Три policy presets

| | **Attribute** | **Feature** | **Component** |
|---|---|---|---|
| При добавлении parasitic | parent **есть в proposal** | parent **added в том же patch** | parent **added в том же patch** |
| При добавлении parent | — | — | component **есть в proposal** |
| При удалении parasitic | parent жив | **parent умирает** | **parent умирает** |

### Attribute — optional decoration

Structural reaction:

- `new_parasitic_requires_existing_parent` — при добавлении attribute host с тем же `id` должен присутствовать в proposal (может быть уже существующим).

Можно повесить на живой host позже (`extend`). Снятие attribute host не убивает.

### Feature — co-born, symbiotic death

Structural reactions:

- `new_parasitic_requires_parent_appears` — parasitic и host должны **родиться в одном patch**;
- `dead_parasitic_kill_parent` — удаление feature убивает host.

Host теоретически может появиться один (нет `parent_appears_requires_*`), но добавить feature «потом» на старый id уже нельзя — strict co-birth.

### Component — mandatory aggregate

Structural reactions:

- `new_parasitic_requires_parent_appears` — co-birth со стороны component;
- `parent_appears_requires_component` — parent не может появиться без component в proposal;
- `dead_parasitic_kill_parent` — удаление component убивает host.

Самая строгая категория: целостный aggregate, не набор независимых частей.

## Где живёт wiring

Policy задаётся в `modules/fQSM.cpp/interface/fQSM/aspect/internals.h` — каждая final category добавляет свой набор structural reactions поверх `Parasitic`.

Реакции — в `modules/fQSM.cpp/interface/fQSM/features/reactions/structural.h`.

## Зачем это языку

- **Attribute** — теги, metadata, поздние наклейки.
- **Feature** — structural capability, которая должна появиться вместе с host и не пережить его смерть.
- **Component** — обязательная часть целого; host без component не рождается.

Это не три названия одного и того же, а три уровня строгости ownership/lifecycle на одном parasitic-каркасе.
