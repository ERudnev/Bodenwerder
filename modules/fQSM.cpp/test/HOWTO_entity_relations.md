# HOW-TO: логика на связях, реакциях и приватном Internals

**Живой образец:** `test/features/entity_relations.cpp`  
**Жанр:** exemplar / cookbook (не assertion-heavy scenario test)

Смежные образцы индекса: `relations_index_build.cpp`, perf — `relations_watch_performance.cpp`.

## Зачем этот тест в системе

Главная роль — **показать HOW-TO**: как класть доменную логику на

- связи между аспектами (`Anchor` / `Custody` / `Affected` / голый `Id` / `optional<Id>`),
- реакции (`customAspectReactions` + `aspect_wide` и др.),
- обратный индекс `ask::relations` по дельте,
- тщательно спрятанный **`Internals`** (не публичный `Actions`).

Сценарий внизу файла — **топливо симуляции** (мир, слоны с разным mood, тики часов).  
В конце — один якорь `EXPECT` (остаётся 5 слонов), не каталог проверок.

Не путать с классическим scenario/BDD: там проверяют исход пользовательского рассказа; здесь рассказ только будит **правила и события** агентной модели.

## Карта слоёв (куда что класть)

| Слой | Что здесь | Чего здесь нет |
|---|---|---|
| `Quantum` | данные и форма связей | бизнес-процедуры |
| `Actions` / `with<>` | публичный API (`with<Meta>` = `BaseActions` = `actions::…::my`) | скрытая политика «почему мир так эволюционирует» |
| `Internals` | приватные Writing-хелперы и тела реакций; home capability только как **`my::`** (без зоопарка глаголов в scope) | то, что зовут снаружи как API |
| `Manipulation` / `Rules` | составные операции над primary (`spawn` слон+хобот) | персистентное состояние |
| `customAspectReactions` | когда мир сам отвечает на дельты | ручной вызов «как будто метод» |

Правило большого пальца: **реакция тонкая** (слушает, решает, зовёт Internals); **Internals** умеет Writing через **`my::`**; снаружи — **`with<Meta>`**; оба смотрят на фасад `actions::…::my` (лестница внутри библиотеки — `Capability`).

## Связи (краткий контракт)

Термины концов связи:

- **holder** — аспект, у которого поле-ссылка;
- **ward** — цель при lookup через `ward` (Custody / Id / Anchor);
- **related** — цель при lookup через `relation` (`Affected`).

### `Custody<Ward>`

Локальная обязанность cleanup: при уходе holder’а (и позже — при release/смене Id) *попытаться* убрать ward.  
Не unique_ptr / shared_ptr: нет уникальности владельца, нет гарантии жизни, stale Id нормален, внешняя смерть ward **не** чистит поле holder’а автоматически.  
Носитель тот же, что `#` / `Identifier` (алиас); опциональность `?` ортогональна.

Lookup: `ward(context, id, &Quantum::myTrunk)` → `const Ward::Quantum*` | `nullptr`.

`optional<Custody<…>>`: `nullopt` или мёртвая цель → тот же `nullptr` (вопрос ward — «есть ли живой квант»).  
Явный отрыв хобота в образце — смена поля `Some → nullopt` (дельта слона), не телепатия по таблице `Trunk`.

Lifecycle cleanup holder→ward: `reaction::structural::custody<…>` (в Behavior, если нужен).

### `Anchor<T>`

Обратная ось зависимости жизни: нет цели — holder не живёт (`reaction::structural::anchored`, если зарегистрирован в Behavior).  
Lookup того же семейства, что Id-поле: `ward(...)`.

Обратный обход «кто смотрит на этот T» в реакциях — через **`ask::relations`**, не полный скан таблицы holder’ов.

### `Affected<T>`

Самая пассивная связь: семантическая обёртка над чужим Id **без** structural lifecycle.  
Тип отдельный от `Identifier` (наследник), чтобы сахар отличал её от `Custody`/`Anchor`.

```cpp
relation(context, id, &Quantum::target)  // via with<> / my:: — const Related::Quantum* | nullptr

const auto* target = my::vital(context, id, &Quantum::target); // miss → remove(self), nullptr
if (not target)
    return;
```

### Голый `Id` / `optional<Id>`

Тот же lookup-контракт, что у Id-поля; без structural и без семантики Affected.  
`ask::relations` принимает `Id`, `Affected`, `optional<Id>` (и Id-алиасы Anchor/Custody) как поле-ссылку.

## `ask::relations` — inbound по слою дельты

Контекст только **`Reacting`**. Семья методов повторяет слой дельты Target:

```cpp
ask::relations<World>(context).updated<Elephant, &Elephant::Quantum::world>()
ask::relations<A>(context).removed<B, &B::Quantum::link>()
```

- пустой слой → пустой индекс **без** скана Watchers;
- иначе индекс только по id из слоя (nullopt-ссылки пропускаются);
- дальше: `.ids(target)` / `.items(target)`.

Не строить «индекс всего мира» руками и не писать `for (change) for (all holders)` — это и есть устаревший квадрат.  
Set-wide правила («пройти всех X») — отдельный случай; индекс не подменяет их, если нет связи на изменившийся тип.

## Review: читать дельту vs писать следствие

- `context.changes<Meta>()` — вход (дельта proposal). Держи `Delta` живой, если сохраняешь view слоя (`removed()` и т.п. — не висячий указатель).
- `context.adjustments<Meta>()` — выходной патч K.
- В теле реакции параметр — **`Reacting`**. В Internals / `with<>` передавай тот же `context`.

Реакции **не** правят proposal напрямую; кладут следствия в adjustments, нормализация крутит `P ← P ⊕ K`.

## Паттерны из образца

### Составной spawn (`Manipulation`)

«Создать слона» = слон + хобот в custody; `spawn(world, mood)` без дефолтов в сигнатуре.

### Writing-хелпер в Internals (ещё не реакция)

```text
Internals::syncTrunkToMood(Writing, Id)
Internals::tearOffTrunk(Writing, Id)  // nullopt + remove Trunk
Internals::boostHappiest / trunklessSadnessAndMelancholy / envyTearOffs — по herd мира
```

### История решений (`History`)

Аспект под `World`: квант `{ world, turn, text }`. Мир про него не знает; остальные аспекты — могут и пишут важные решения через `History::Internals::note` (имена слонов — только когда решение про конкретного слона).  
В конце стимула dump в лог закомментирован — не часть CI.

### Зависть и экология mood (реакция на World)

`aspect_wide<Elephant, World>` на тик часов:

1. `ask::relations<World>(…).updated<Elephant, &world>()` → herd слонов **этого** мира;
2. sync углов хобота от mood по herd;
3. зависть внутри herd (`envyAngleGapDegrees`);
4. самый весёлый в herd → `+1`;
5. без хобота → `−1` mood; `mood < 0` → `remove`.

Параллельные `remove` одного Trunk в одном патче допустимы — нормализация схлопывает.

### Disappointment

- `Rules::afflict` + `standardAfflictionHours` (= 3).
- Часы мира: тот же inbound World→Elephant, затем Disappointment с `target ∈ herd` (нет поля World на Disappointment — фильтр по слонам тикающего мира).
- Потеря хобота: `field_event(…, &myTrunk).removed` → `afflict` (обход только `changes<Elephant>()`, без скана таблицы).

### Реакция на свой квант

`reaction::aspect_wide<Self, …>(&Internals::handler)` + в handler’е обход `changes<…>().updated()` / `addedOrUpdated()` / `removed()` и `adjustments` / `with<>::modify|remove`.  
Вычислимое «событие поля»: `field_event(change, &Quantum::member)` → `{ old, now, changed }` (для `optional` ещё `appeared` / `removed`).  
`constraint::element` — про **правку** кванта, не про самоудаление.

### Стимул внизу файла

Мир → десять слонов (`elephant1`…, mood 0..9) → тики → `EXPECT`: остаётся 5 слонов.

## Анти-паттерны (для агента и автора)

- Рано набивать expects и побочные сцены «чтобы было что проверить».
- Публиковать политику в `Actions`, которую никто не должен звать руками.
- Дефолтные аргументы в API «для удобства» (в этом коде явно не любят).
- Второй `context` в ручном `get`+`find`, если достаточно `my::ward` / `with<>::ward`.
- Голые `get`/`modify`/`vital` в Internals (зоопарк в scope) — только `my::`.
- `throwing_before` / сырой `Change.before` в доменных реакциях — для слоёв есть `old`/`now`.
- Авто-обнулять поле holder’а при смерти ward (для custody — запрещено контрактом).
- `for (change) for (all holders)` вместо `ask::relations` при наличии поля-ссылки на изменившийся тип.
- Сохранять `changes().removed()` (view) без живой `Delta` — dangling.

## Как наращивать документ

Пока `entity_relations.cpp` растёт, сюда дописывают только то, что **закрепилось как приём** в чате/коде.  
Черновые гипотезы и одноразовые эксперименты в HOW-TO не тащат.
