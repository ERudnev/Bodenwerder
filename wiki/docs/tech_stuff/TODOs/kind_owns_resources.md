# Вид владеет полкой, сценарий только расставляет

## Намерение

Знание «какие ассеты мне нужны» принадлежит **виду** (Bullet, Mount, Rock) или **киту** определений (полка JSON-маунтов, чертежи), а не `Scenario::loadResources` и не свалке `Game::addAssets`.

Сценарий — только *где / сколько / какой позой*. Высокоуровневый spawn уже не принимает Family/Packed.

## Что уже намекает на это

- `Bullet`: Global держит Family; `bind` сам находит `Eltanin::projectiles` / `shell_30mm`; сценарий зовёт `spawnShell30mm`.
- `MountCatalog` / `BlueprintCatalog` — киты с диска; каждый Mount в JSON сам указывает `tempMesh.pack` + `entry`.
- Три сценария до сих пор копируют rock/boulder/crust shaders — след «сценарий владеет ресурсами».

## Слои (не смешивать)

1. **Регистрация полки** — имена, meshpack, файлы. Сцены нет, GPU не нужен. Хозяин — вид или каталог вида.
2. **Bind прототипов** — Family на GPU, Mount как feature на Unit. После materialize. Family сейчас ещё требует Root (`group<Family> of Root`).
3. **Populate** — только spawn/позы.

`Game::bindGameEntities` — явный список видов («мир умеет пули / маунты / чертежи»), не plugin-scan.

## Чего не делать

- Не называть вид «мини-сценарием»: сценарий = расстановка.
- Не требовать сцену, чтобы определение существовало (JSON-маунт уже определение).
- Не оставлять регистрацию паков (`Eltanin::devices`, `Eltanin::projectiles`) только в `Game::addAssets`, если на полку ссылается вид.

## Практичный шаг

Полки регистрирует тот, кто на них ссылается (`Bullet` / `MountCatalog`); `addAssets` только вызывает их `register`. `bindGameEntities` вызывает их `bind`. Сценарий к файлам не прикасается.
