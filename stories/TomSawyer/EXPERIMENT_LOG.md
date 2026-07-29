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
