# Retrospection / persistency schema: долг после миграции

После завершения миграции persistency (workshop → library → unit-test / пустой workshop) — сразу вернуться сюда.

См. также backlog fQSM: `modules/fQSM.cpp/_management/backlog.tome` → `persistency_away_from_main_schema`.

## 1. Карта retrospection ≠ SQLite

`processing/persistency/database/retrospection.h` лежит под `database/`, но по смыслу это **карта формы аспекта** (пути к полям, quantum/global/collections, kind), а не SQLite.

Сейчас файл всё же завязан на БД:

- `bindLeaf` / `readLeaf` / `appendElement` принимают `sqlite3_stmt*`
- фабрики `column` / `collection` тянут `database/sql.h`
- type erasure через `void*` + C-указатели на функции (в ревью так не живёт долго)

### Цель разреза

| Слой | Содержание |
|------|------------|
| `persistency/retrospection.h` (уровень выше) | карта: пути, списки полей, kind; **без** sqlite и без bind-кодеков |
| `persistency/database/` | SQL-атомы, `sqlite3_stmt`, engine DDL/DML |

Склеивание: sqlite-бэкенд пишет/читает по kind + типу поля (или отдельный sqlite-view карты), без `sqlite3_stmt*` в общей retrospection.

Заодно убрать `void*`-erase с пути (типобезопасный path или нормальный typed erase в духе `Binding` / `std::function` с живыми типами).

Итог: ту же карту можно кормить в JSON / бинарь / другой бэкенд, не таская `sqlite3.h` в общий persistency.

## 2. Ошибка: persistency внутри главной Schema

**Пихать персистентные вещи в главную схему мира было ошибкой.**

Сейчас: отдельная `persistency::Schema` / `Graph` (workshop уже разводит world merge и `persist::merge`); Archivist принимает Schema доп. параметром.
Ещё убрать: обобщить retrospection; добить unit-test / пустой workshop.

### Цель

- Отдельная **схема персистентности** — неполная по типам (только то, что сохраняем); может быть связана с основной, может нет.
- **Основная схема об persistency не знает.**
- Кто получает информацию о типах через Writing/Reading, для архива получает **дополнительные параметры** (отдельный handle / palette / persist-schema), а не поле на `schema->nodes`.

Archivist и load/save смотрят в persist-схему (или явный параметр контекста), а не в `context->schema` как единственный источник правды про «что архивировать».
