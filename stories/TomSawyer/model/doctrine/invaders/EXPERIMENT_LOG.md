# Agent × Q1 — Space Invaders design experiment

Лог эксперимента: проектирование игрушки **языком Q1** (doctrine) с синхронной C++-проекцией, руками агента.
Папка: `stories/TomSawyer/model/doctrine/invaders/`.

Формат записи:

```
## YYYY-MM-DD — краткий заголовок
### Запрос
### Действие
### Выводы / наблюдения
```

---

## 2026-07-28 — старт лога + контекст дуги

### Запрос
Завести MD-лог эксперимента «дизайн игры агентом на Q1»; дальше — фича Gun как агрегат Player.

### Действие
Создан этот файл. Ниже — сжатая ретроспектива уже сделанного до лога (чтобы следующие записи были инкрементальными).

### Выводы / наблюдения
- Doctrine и C++ легко **разъезжаются**, если у сущности нет пары файлов (`Something` → `GameObject.h/.cpp` — урок).
- Полезный контракт: **роль** (`feature of GameObject`) владеет appearance (`always` index/scale/bank/tint); **GameObject** держит живой `#Sprite?`.
- Транзакционная модель ломает STL-рефлекс «не удаляй в цикле» / `doomed[]`; эталон — destroy внутри рефлекса объекта.
- Watch должен быть тонким fan-out; тело — в reflex/action объекта (`advance` → `advanceMotion`).
- Тюнинг «игрушки» (камера в меню, scale, bank, tint, скорость пуль) удобно вести через `always` ролей, не через глобальный пак.

---

## 2026-07-28 — GameObject, appearance, tint (ретро)

### Запрос
(накопленное) Масштаб/ориентация/цвет как решение актора; переименование Something → GameObject.

### Действие
- `gameObject.q1.types` + `gameObject.h/.cpp`
- Player/Alien/Shot — `feature of GameObject`; Session.player — `#GameObject?`
- Alien `?sprite_tint(cell)`; spawn прокидывает additive RGB

### Выводы / наблюдения
- Аддитивный tint (`albedo = 1 + tint`) хорошо читается на Kenney-атласе при amp ~0.35.
- Инстансный tint от `cell` даёт «каждый свой цвет» без поля в quantum.

---

## 2026-07-28 — entity Gun, custody у Player, fire как Action

### Запрос
Выделить пушку из Player отдельным entity; `custody` в кванте корабля (агрегат); стрельба — Action пушки.

### Действие
- Doctrine: `gun.q1.types` (`entity Gun`, `>fire(#Session, muzzle)->#?`); Player — `gun: custody<Gun>`, `!tryFire` только детект края → Gun.
- C++: `gun.h` / `gun.cpp` (`Gun::Actions::fire`); `Player::Quantum::gun`; `structural::custody`; bootstrap создаёт Gun до extend Player.
- Лог эксперимента: `EXPERIMENT_LOG.md` в этой папке.

### Выводы / наблюдения
- Агрегат через `custody<>` читается в Q1 лучше, чем «cooldown в Player»: корабль двигается, пушка стреляет.
- Watch `tryFire` остаётся на Player (он слышит Window/World); эффектор стрельбы — публичный `>` на Gun (не Internals reflex).
- Снова важна **пара файлов** doctrine↔C++ (`gun.q1.types` ↔ `gun.h/.cpp`), иначе сущность снова «потеряется» в чужих translation units.
- `>fire(...)->#?` в C++ удобно вернуть как `maybe<GameObject::Id>` (Shot — feature of GameObject), без циклического include combat в gun.h.

---

## 2026-07-28 — dual cooldown: mech + температура (°C)

### Запрос
Два кулдауна на пушке: быстрый механический (~4 выстр/с) и накопительная температура (°C, clamp 0…~500), остывает со временем, стрельба только ниже порога, выстрел +~100°C — «дозаряжаемый магазин».

### Действие
- Doctrine `Gun`: `mech_cooldown_steps`, `temp_max` / `fire_below` / `heat_per_shot` / `cool_celsius_per_sec`; quantum `mech_ready_at`, `temperature_celsius`, `cool_step_carry`; `!cool(~World)`; `session: affects<Session>`.
- `fire`: mech gate + `temperature < fire_below`; убран gate «одна player-пуля в воздухе».
- C++: `Gun::Internals::cool` через World→Session→Gun; bootstrap/reset обнуляет оба лимитера.

### Выводы / наблюдения
- Механика «магазина» лучше сидит на агрегате Gun, чем на Shot_group (one-alive): несколько пуль в полёте + тепло как ресурс.
- Остывание требует собственного `!cool(~World)` и `affects<Session>` — иначе Gun не привязан к шагу мира.
- Sub-degree cooling при integer °C → `cool_step_carry` (шаги×rate / 1000); иначе при dt=1 step скорость <1000°C/s не двигалась бы.
- Порог `fire_below` (400) отдельно от `temp_max` (500): выстрел может увести выше порога; нужен откат остыванием — лёгкий hysteresis без отдельного флага overheat.

---

## 2026-07-28 — bugfix: остывание молчало (пачка → тишина)

### Запрос
Пачка выстрелов, потом наглухо умолкает.

### Действие
`!cool` ошибочно индексировал `relations<Session>.updated<Gun>`: на тике World Session не в дельте → пустой индекс → температура залипала ≥ `fire_below`. Исправлено: `world: affects<World>` + `relations<World>.updated<Gun>`.

### Выводы / наблюдения
- `ask::relations<T>.updated<W>` — только watchers на **id из дельты T**, не «все, кто смотрит на живые T». Для `~World` линк должен быть на World (как у Elephant), а не через тихий Session.
- Симптом «магазин разрядился и не перезаряжается» = остывание не бежит; сначала проверять inbound index, не баланс °C.

---

## 2026-07-28 — ImGui плашка Gun (оба кулдауна)

### Запрос
Отдельный игровой UI: крупно показать механический и температурный кулдауны (насколько ImGui позволит).

### Действие
`story.ui.cpp`: окно `GUN` (View → Gun), правый верх; ProgressBar RATE (ready/остаток) + HEAT (°C, цвет, метка порога `fire_below` на баре). Чтение Session→Player→Gun, без правок doctrine.

### Выводы / наблюдения
- Для «игрушечного» фидбека механики хватает Dear ImGui ProgressBar + `SetWindowFontScale` + `GetWindowDrawList` (черта порога); отдельный in-world HUD пока не нужен.
- Плашка читает живой quantum Gun — хороший смоук-тест, что cool/fire реально крутятся.


---

## 2026-07-28 — hold-to-fire

### Запрос
Автоогонь при удержании Space/W (не только rising edge).

### Действие
`Player::tryFire` семплирует held keys; cadence/heat по-прежнему в `Gun.fire`.

### Выводы / наблюдения
- Детект ввода (hold vs edge) — забота Player-watch; лимитеры остаются на Gun. Разделение ролей держится.

---

## 2026-07-28 — GameObject.hitpoints (общий пул для feature)

### Запрос
Единый hitpoints на GameObject; все feature роли получают HP автоматически; убрать Alien.alive.

### Действие
Doctrine/C++: `hitpoints` + `>takeDamage` на mom; роли — `max_hitpoints` в `always`; spawn/combat/fleet через `GameObject.alive` / `takeDamage`.

### Выводы / наблюдения
- Смысл shared Id: HP живёт на mom, роли не дублируют `alive`/`hp`.
- `Session.lives` ≠ hull HP: lives = continues; после deplete при lives>0 hull снова `max_hitpoints`.

---

## 2026-07-28 — critique: `?alive` не в `all`

### Запрос
`?alive()->bool` в разделе `all` читается как «живо ли всё множество GameObject».

### Действие
Перенесён в `one` (предикат этого экземпляра). `>takeDamage` остаётся в `all` как Action по адресованному id.

### Выводы / наблюдения
- В Q1 `all` + безаргументный `?` легко читается как set-wide; instance-query — в `one`, рядом с полями кванта.

---

## 2026-07-28 — sync doctrine↔C++ (хвост)

### Запрос
Дотянуть расхождение: константы, Volley.fire, Playfield, Alien.worldPos.

### Действие
C++: `hit_half` / Fleet march layout / `start_lives` / `wave_clear_steps` / `Volley::fire`+`muzzle_drop` / `Playfield::contains|install` / `Alien::worldPos`. Doctrine уже была выровнена ранее (без Shot.fire*).

### Выводы / наблюдения
- После чистки doctrine «впереди» остаются только непроецированные always — дотягивать C++ именами, не откатывать значения в магию.
