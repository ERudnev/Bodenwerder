# HOW-TO: логика на связях, реакциях и приватном Internals

**Живой образец:** `test/features/entity_relations.cpp`  
**Жанр:** exemplar / cookbook (не assertion-heavy scenario test)

## Зачем этот тест в системе

Главная роль — **показать HOW-TO**: как класть доменную логику на

- связи между аспектами (`Anchor` / `Custody` / `Affected`),
- реакции (`customAspectReactions` + `aspect_wide` и др.),
- тщательно спрятанный **`Internals`** (не публичный `Actions`).

Сценарий внизу файла — **топливо симуляции** (мир, слоны с разным mood, тики часов), а не «юзкейс пользователя».  
Expects — **мало и в конце** (когда появятся): якоря «правила живы», не каталог проверок.

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

### `Anchor<T>`

Обратная ось зависимости жизни: нет цели — holder не живёт (см. `structural::anchored`).  
Lookup того же семейства, что Id-поле: `ward(...)`.

### `Affected<T>`

Самая пассивная связь: семантическая обёртка над чужим Id **без** structural lifecycle.  
Тип отдельный от `Identifier` (наследник), чтобы сахар отличал её от `Custody`/`Anchor`.

```cpp
relation(context, id, &Quantum::target)  // via with<> / my:: — const Related::Quantum* | nullptr

const auto* target = my::vital(context, id, &Quantum::target); // miss → remove(self), nullptr
if (not target)
    return;
```

### Disappointment (механизм в образце)

- `Rules::afflict` + `standardAfflictionHours` (= 3, нарратив «−3»).
- Часы мира → dt из `change.old` / `change.now` → `vital` related-слона → mood и `remains`; `remains <= 0` → remove.
- Потеря хобота у слона → `field_event(change, &Elephant::Quantum::myTrunk).removed` → `afflict` на потерпевшего.
- В сценарии пока **не** развешиваем вручную — только механизм (отрыв сам может навесить).

## Review: читать дельту vs писать следствие

- `context.changes<Meta>()` — вход (дельта proposal).
- `context.adjustments<Meta>()` — выходной патч K (раньше путающе назывался `reaction()`).
- В теле реакции параметр — **`Reacting`**. В Internals / `with<>` передавай тот же `context`.

Реакции **не** правят proposal напрямую; кладут следствия в adjustments, нормализация крутит `P ← P ⊕ K`.

## Паттерны из образца

### Составной spawn (`Manipulation`)

«Создать слона» = слон + хобот в custody; `spawn(world, mood)` без дефолтов в сигнатуре.

### Writing-хелпер в Internals (ещё не реакция)

```text
Internals::syncTrunkToMood(Writing, Id)
Internals::tearOffTrunk(Writing, Id)  // nullopt + remove Trunk
Internals::boostHappiest / trunklessSadnessAndMelancholy
```

### История решений (`History`)

Аспект под `World`: квант `{ world, turn, text }`. Мир про него не знает; остальные аспекты — могут и пишут важные решения через `History::Internals::note` (имена слонов — только когда решение про конкретного слона).  
В конце стимула — dump в лог, отсортированный по `turn`.

### Зависть и экология mood (реакция на World)

`aspect_wide<Elephant, World>` на тик часов, по порядку:

1. sync углов хобота от mood;
2. зависть: живой хобот видит чужой угол выше своего более чем на `envyAngleGapDegrees` → `tearOffTrunk` жертвы (без хобота в зависти не участвует);
3. самый весёлый (max mood) → `+1`;
4. без хобота (`nullopt`) → `−1` mood; `mood < 0` → `remove` (тоска).

Параллельные `remove` одного Trunk в одном патче допустимы — нормализация сама схлопывает.

### Реакция на свой квант

`reaction::aspect_wide<Self>(&Internals::handler)` + в handler’е обход `changes<Self>().updated()` (узкий тип с `old`/`now`) / `addedOrUpdated()` (`now`, опционально `old`) и `adjustments<…>().put_deletion` / `put_modification`.  
Вычислимое «событие поля»: `field_event(change, &Quantum::member)` → `{ old, now, changed }` (поверх `Updated`); для `optional` ещё `appeared` / `removed` (поля, не методы).
`constraint::element` — про **правку** кванта, не про самоудаление.

### Стимул внизу файла

Мир → десять слонов (`elephant1`…, mood 0..9) → тики → один якорь: остаётся 5 слонов.  
Лог `History` внизу закомментирован — память / корм для LLM, не часть CI-контракта.

## Анти-паттерны (для агента и автора)

- Рано набивать expects и побочные сцены «чтобы было что проверить».
- Публиковать политику в `Actions`, которую никто не должен звать руками.
- Дефолтные аргументы в API «для удобства» (в этом коде явно не любят).
- Второй `context` в ручном `get`+`find`, если достаточно `my::ward` / `with<>::ward`.
- Голые `get`/`modify`/`vital` в Internals (зоопарк в scope) — только `my::`.
- `throwing_before` / сырой `Change.before` в доменных реакциях — для слоёв есть `old`/`now`.
- Путать камерный `controller` / будущий `aspect::Controller` с полем `custody`.
- Авто-обнулять поле holder’а при смерти ward (для custody — запрещено контрактом).

## Как наращивать документ

Пока `entity_relations.cpp` растёт, сюда дописывают только то, что **закрепилось как приём** в чате/коде.  
Черновые гипотезы и одноразовые эксперименты в HOW-TO не тащат.
