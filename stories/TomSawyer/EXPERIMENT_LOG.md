# Agent × Q1 — Stargun experiment

Папка: `stories/TomSawyer/`.

Формат записи:

```
## YYYY-MM-DD — краткий заголовок
### Запрос
### Действие
### Выводы / наблюдения
```

---

## 2026-07-29 — старт лога

### Запрос
### Действие
### Выводы / наблюдения

---

## 2026-07-29 — GameObject + Stone ×100

### Запрос
В начальной сцене — сотня камней. Камень = feature над GameObject; для него выбирается спрайт. Подключить библиотеку `assets/TomSawyer/sprites`, выбрать какой-нибудь спрайт.

### Действие
- Doctrine: `GameObject.sprite`; `feature Stone of GameObject` (`sprite_index=163` meteorGrey_big1, `sprite_scale=0.5`)
- C++: `gameObject.h` (sprite), `stone.h`; assets library → `TomSawyer`; `setup` спавнит 10×10 Stone
- CMake: `stone.q1.types`

### Выводы / наблюдения

---

## 2026-07-29 — Stone: random pose + tint

### Запрос
Камни не сеткой, а рандомно; индивидуальный цвет актера; случайный поворот вокруг своей оси при создании (без анимации).

### Действие
`setup`: 100 Stone с `mt19937` — random pos в поле, additive `Sprite.tint`, random `HPB.z` bank. Doctrine не трогал.

### Выводы / наблюдения

---

## 2026-07-29 — Stone: dark tint + size quantum

### Запрос
Убрать разноцветный tint; слегка уводить в тёмный; случайный масштаб 50…200% от текущего; размер камня — параметр кванта.

### Действие
- Doctrine/C++: `Stone.size: float` в `one`
- Tint: отрицательный серый additive (~−0.08…−0.22)
- Scale: `sprite_scale * size`, `size` ∈ [0.5, 2.0]

### Выводы / наблюдения

---

## 2026-07-29 — GameObject size/mass + World collisions

### Запрос
Добавить коллизии в мир; перенести размер в GameObject; масса (float) у любого GO, пока = size.

### Действие
- Doctrine/C++: `GameObject.size` + `GameObject.mass`; убран `Stone.size`
- Spawn: `mass = size`
- World: `!resolveCollisions(=one)` — круги `radius = size * 40`, разведение по массам на росте `step`

### Выводы / наблюдения

---

## 2026-07-29 — collider = sprite diameter

### Запрос
Удостовериться, что физический размер коллайдера соответствует диаметру спрайта; при необходимости перебалансировать.

### Действие
Убран magic `size * 40`. Радиус из atlas entry × live `Sprite.scale`: `0.5 * max(w,h) * scale` (как в `sprite.vert.glsl`). Для meteorGrey_big1@size=1: было r≈40, стало r≈25.25.

### Выводы / наблюдения

---

## 2026-07-29 — feature Physical of GameObject

### Запрос
Сформировать физический компонент; не добавлять новых фич — перенести нужные параметры из GameObject.

### Действие
- Doctrine: `feature Physical` с `size`/`mass`; убраны с `GameObject`
- C++/spawn/collisions читают `Physical`

### Выводы / наблюдения

---

## 2026-07-29 — Physical.resolveCollisions (all-op)

### Запрос
Резолв коллизий переехать в `Physical` как публичную `all`-операцию; итератор по всем Physical в мире.

### Действие
- Doctrine: `Physical.all =resolveCollisions()`; убран `World.!resolveCollisions`
- C++: `Physical::Actions::resolveCollisions(Writing)` — iterate `aspect<Physical>`
- World.`advanceStep` после роста `step` зовёт `with<Physical>::resolveCollisions`

### Выводы / наблюдения

---

## 2026-07-29 — materialized Inertia

### Запрос
Материализовать `feature Inertia of Physical`, но не навязывать его всем Physical.

### Действие
- C++ projection: `Inertia : Feature<Inertia, Physical>`
- Quantum: `vel: vec3`, `saturation: float`
- Public all-op: `Inertia::Actions::update(Writing)`
- `SpriteTest::schema()` теперь включает `Inertia`

### Выводы / наблюдения
- Семантика `update()` пока не задана в проекте, поэтому проекция оставлена честно незавершённой через `_INCOMPLETE_`, без выдумывания движения.

---

## 2026-07-29 — Inertia.update semantics

### Запрос
Объект не знает источник `vel`; интегрирует `pos += vel * dt`, затем `vel *= (1 - saturation)`.

### Действие
- `Inertia::update`: dt = 1 World.step; Node.pos += vel; vel *= (1 - saturation)
- World.`advanceStep`: на каждый advanced step → `Inertia.update`, потом `Physical.resolveCollisions`
- Камни по-прежнему без `Inertia` (слой опционален)

### Выводы / наблюдения

---

## 2026-07-29 — rammer stone smoke test

### Запрос
Слева крупный камень со скоростью врезается в поле; коллизия и инерция параллельны.

### Действие
- Story: поле 100 камней — только Physical; слева rammer + Inertia
- Physical/GameObject не трогал: импульс в vel через коллизии не протаскивал

### Выводы / наблюдения

---

## 2026-07-29 — Player ship + thrusters → Inertia

### Запрос
Кораблик игрока: Feature с клавиатуры, Inertia, коллизии; двигатели пишут в вектор скорости инерции.

### Действие
- Doctrine/C++: `feature Player`; `all =applyThrusters()`
- World loop: `applyThrusters` → `Inertia.update` → `resolveCollisions`
- Spawn: Player + Physical + Inertia внизу; WASD/arrows

### Выводы / наблюдения

---

## 2026-07-29 — Gun + Shot (SI01 idea) + SHIP HUD

### Запрос
Пушка у корабля (идея из SI01); HUD пушки в UI корабля.

### Действие
- Doctrine/C++: `entity Gun` (mech+heat, `!cool(~World)`, `>fire`); `feature Shot` (Inertia flight, `resolveHits`/`cullExpired`); `Player.gun: custody<Gun>`, `=tryFire` (Space)
- World loop: thrusters → tryFire → inertia → Shot hits/cull → collisions → camera
- ImGui `SHIP`: HULL + RATE + HEAT в одной плашке

### Выводы / наблюдения

---

## 2026-07-29 — AnimatedDecay + Sprite.opacity

### Запрос
`attribute AnimatedDecay of GameObject`: живёт N шагов, твинит альфу спрайта → 0, затем убивает host. World раздаёт всем с HP ≤ 0.

### Действие
- Doctrine: `AnimatedDecay` сразу после `GameObject`; `GameObject.=destroy()`
- rmmr: `Sprite.opacity` + `u_opacity` в sprite shader / material / renderer
- World: `grantToDepleted` → `update` после Shot hits; Shot больше не уничтожает target сразу (только HP → 0)

### Выводы / наблюдения

---

## 2026-07-29 — fire: requestFire queue + flushPending Writing

### Запрос
Transaction rejected: Node must appear with new Sprite (при стрельбе).

### Действие
- Причина: `createSpriteActor` из `World::advanceStep` (Reacting) ломает Feature-structural для Sprite/Node
- `Gun.requestFire` только ставит в очередь (mech+heat); `Gun.flushPending` спавнит спрайты из Writing (`drawUi`)

### Выводы / наблюдения
