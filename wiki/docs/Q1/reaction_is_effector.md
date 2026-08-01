В Q1 реакция — это **watch** (пробуждение по scope + условие). Она может быть цельной или заканчиваться хвостом, который вызывает изменение состояния.

## Формы

| Форма | Запись | Смысл |
|--|--|--|
| цельная | `!name(scope)` | детект и эффект в одном теле |
| watch + action | `!name(scope)->>op(...)` при явном `>op` | публичный эффектор |
| watch + reflex | `!name(scope)->>op(...)` без `>op` | тот же хвост; `op` только в Internals |
| watch + command | `!name(scope)->=op(...)` | хвост `=`; публичный или приватный по тому же правилу резолва |

`->>` = `->` от `>`: реакция «возвращает» вызов эффектора, не обычное значение. `->=` = `->` от `=`.

Реакция **не** может завершиться `-> T` с обычным типом: нет вызывающего кода, которому вернуть значение. Единственный осмысленный «return» реакции — вызов action / reflex / command (или цельное тело без хвоста).

## Action vs reflex

- **action** — публичный `>` в теле аспекта; можно звать снаружи.
- **reflex** — вылитый action по форме, но **не** для ручных вызовов: появляется из `->>`, если публичного `>` с этим именем нет. Проекция кладёт его в Internals и **никогда не изобретает** `>` в Actions из хвоста.

То же для `->=`: не изобретать публичный `=` из хвоста.

## one / all

- **one** — реакция **объекта** (обычный случай). Scope: `=one` / `-one` / `>one` или стимул `~` / `~Type`.
- **all** — реакция **множества как множества** (редко).

Пример (эталон `ReactionSketch`):

```
entity ReactionSketch
  one
    value: string
    !normalize(=one)
    !watch_clock(~Clock)->=field_update()
    !watch_sample(~SampleEntity)->>reflex()
    !watch_trigger(~SampleEntity)->>field_action()
  all
    >field_action()
```

Здесь `->>field_action()` — action (есть `>field_action`), `->>reflex()` и `->=field_update()` — приватные хвосты (reflex / private command).

Живой игровой образец после «долга» оси: `stories/TomSawyer/doctrine` + C++ `stories/TomSawyer/model/tommy` (Player/Shot: watch → Internals reflex; Fleet/Volley/World часто цельный `!`).
