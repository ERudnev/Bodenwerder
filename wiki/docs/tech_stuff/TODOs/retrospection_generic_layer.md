# Retrospection: обобщить и вынести из `database/`

После завершения миграции persistency (workshop → library → unit-test / пустой workshop) — сразу вернуться сюда.

## Долг

`processing/persistency/database/retrospection.h` лежит под `database/`, но по смыслу это **карта формы аспекта** (пути к полям, quantum/global/collections, kind), а не SQLite.

Сейчас файл всё же завязан на БД:

- `bindLeaf` / `readLeaf` / `appendElement` принимают `sqlite3_stmt*`
- фабрики `column` / `collection` тянут `database/sql.h`
- type erasure через `void*` + C-указатели на функции (в ревью так не живёт долго)

## Цель

Разрезать слои:

| Слой | Содержание |
|------|------------|
| `persistency/retrospection.h` (уровень выше) | карта: пути, списки полей, kind; **без** sqlite и без bind-кодеков |
| `persistency/database/` | SQL-атомы, `sqlite3_stmt`, engine DDL/DML |

Склеивание: sqlite-бэкенд пишет/читает по kind + типу поля (или отдельный sqlite-view карты), без `sqlite3_stmt*` в общей retrospection.

Заодно убрать `void*`-erase с пути (типобезопасный path или нормальный typed erase в духе `Binding` / `std::function` с живыми типами).

Итог: ту же карту можно кормить в JSON / бинарь / другой бэкенд, не таская `sqlite3.h` в общий persistency.
