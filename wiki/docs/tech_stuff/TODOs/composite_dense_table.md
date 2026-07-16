# Composite: DenseTable вместо `unordered_map`

Контекст (raidenmamare / fQSM, 2026-07): Writing×N × |schema| жрёт кадр; skip пустых линий в norm/integrate/absorb уже помогает. Разреженный Patch — отдельно и пока не цель.

## Претензия (Morta)

В любом композите (`Patch` / `Future` / …) линия ищется и обходится через `unordered_map<Rtid, …>`.

Хочется:

- контейнер линий — `std::vector<>` (плотный);
- в schema — индекс типов в `[0..n)`;
- «дай linear:: этого Meta» — **O(1)** по слоту, без hash по `Rtid`.

## Заметки со стороны агента

- Это **облегчить плотное**, не sparse: семантика «линия на каждый aspect schema» может остаться.
- Schema один раз держит `Rtid → slot`; композиты индексируются слотом.
- Копии `Future` сами по себе дешёвыми не станут (всё ещё O(n) указателей); выигрыш — ctor/lookup/iterate.
- **А нахрена в контейнере вообще `shared_ptr` / `ref<>` на каждую линию?** Слоты могли бы владеть значением (или уникальным ptr), без atomic refcount на каждый aspect при каждом ctor композита.
- Замеры лучше на Release; debug сильно раздувает Writing.

Бенч: `fQSM` `cascade_performance` (8 аспектов, каскад реакций, N× modify).
