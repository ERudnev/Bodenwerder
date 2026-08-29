# Патчлет не помнит, что его добавили

## История

Eltanin: мёртвая плитка Construct (`cohesion <= 0`) должна родить `Scrap` (Body+Solid, Thing+Scrap) в том же Stewarding, где физика уже открыла Direct на Body/Thing/Crystal.

Structural `new_parasitic_requires_parent_appears` смотрит `delta.added()`. Create шёл через `put_add`, но в dirty-сессии новый id не попадал в `added()` — классификация «это add?» смотрела, есть ли квант в таблице. После Direct строка уже там → update/taint, патчевый проход ещё и `contains` скиппает. Транзакция отвергалась, GL от `Mesh::replace` уже успевал умереть.

Это не «логика в неправильном контексте» и не дыра в Direct-итераторе как модели. `put_add` в момент записи знает, что это рождение, и сразу врёт: кладёт `Patchlet::modification`. У патчлета есть `tombstone` (смерть) и `verified` (Скарлетт), рождения нет.

## Что не делать

- Не кормить Direct «origin-таблицей» вместо overlay — это вторая таблица в модели, не патч.
- Не чинить через `addedOrUpdated()` — туда входит taint, Solid на старом Body пройдёт.
- Не очереди shed и не `world.branch` изнутри Dock.

## Обход (Eltanin, сейчас)

Физика — Stewarding/Direct, только числа. Логика (shed, spawn scrap, kraken пуль) — отдельный Writing после закрытия Dock. Факты (когезия, `bornAt`) уже в модели.

## Фикс

Поле на патчлете: это появление. `put_add` его ставит. `Delta::added` верит патчлету, не `state.find`.

Taint без патчлета по-прежнему не add. Merge: рождение в патче переживает поздний modify того же id. Direct, даже если строка уже в таблице, не переклассифицирует appeared в update.

Симметрия tombstone. Cannonball, не новый API Future.
