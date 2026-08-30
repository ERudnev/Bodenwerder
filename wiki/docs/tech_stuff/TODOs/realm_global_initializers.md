# Realm рождается непустым: инициализаторы Global

Связано: [`needless_invariant_checks.md`](needless_invariant_checks.md).

## Сделано (schema-born)

Q1: `always >assemble() -> all` — статический конструктор сумки `all`. C++: `Always::assemble(SettingUp&) -> Global`. `SettingUp` чеканит только конструктор `Realm`; `Writing` получают явно (`setup.writing()`). Пока ctor не закончился, мир ненаблюдаем.

Пилот: `eltanin::locality::Thing.scene` — `#rmmr::scene::Root` без `?`. Assembler зовёт `scene::Interface::createScene`. Spawn больше не спрашивает «есть ли сцена».

Тестовая схема без assembler-аспектов по-прежнему даёт пустые линии (default-constructible Global). Схема с Thing без Root в схеме — ошибка сборки мира.

## Осталось

- `Core` / `Clock` / `Assets.singleton` — сентринел-Id до `Interface::create` (нужны `path` / `GLVer`). SettingUp без сессии их не соберёт.
- Явный DAG assembler-ов по полям `#T` на Global (сейчас порядок узлов схемы; для одного Thing достаточно).
- Persistency / загрузка с диска — другой вход, assembler не гонять.

## Чего не делать

- Не маскировать дыру вечным `optional` + `if (not scene) refuse` как дизайн.
- Не объявлять `Product::setup` / `populateWorld` мета-конструктором fQSM.
- Не тащить GLFW/GPU в инициализатор Thing.
- Не смешивать с persistency.
