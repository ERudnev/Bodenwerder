# Тупик: RDB + JSON через Retrospection «в лоб»

2026-07-24

Лабораторная запись. Цель — зафиксировать, что сделали, где упёрлись и какие выводы брать с собой, когда снова вернёмся к архиву (RDB / JSON / binary).

## Постановка

Хотелось одного `describe` на доменный `Meta` (`Retrospection<T>`), чтобы из него:

- JSON-архив каталога ассетов Toy;
- параллельно SQLite (dual-write) как «та же форма, другой бэкенд».

Идея: cereal-подобная форма → два движка pQRF без дублирования знания о структуре.

```cpp
// желаемая картина
template<>
struct Retrospection<material::Asset> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("…::material::Asset");
        d.one(collection<&Quantum::techniques, Technique>("techniques"));
    }
};

archivist.save(world, schema);   // json
archivist.save(world, schema);   // sqlite — «тот же» schema
```

## Что успели построить

Механика не была пустой. До заморозки дошли до рабочего dual-write на nested material techniques:

- **форма** — `one` / `all` × `field` / `collection`, nested `Retrospection` на продуктах;
- **JSON** — `value_form.h` (leaf / pair / nested product);
- **DB** — `form_tree.h`: nested `collection<>` → дочерние таблицы; nested product → dotted columns (`program.id`, …); map key как колонка (`pass`);
- **срез каталога** — типы ассетов + рецепты на стороне rmmr, Toy только path + seed/load;
- **Toy** — seed / load / save поверх Archivist.

Срез каталога в голове выглядел так:

```text
Manager / Unit / (маркер Assets)
  + texture|shader|material|shadow|geometry::Asset
  + Loader / Generator / Composer / Allocator
Runtimes / GPU — вне архива
```

Инфраструктура (trait, concept `musthave::Retrospection`, движки) осталась в дереве. Доменные специализации rmmr вырезаны; у Эталона Q1 — архивный `aspects.s11n.h` (никто не include). Живые формы — в юнит-тестах fQSM.

## Симптом тупика

Технически save/load «собирались». Боль болела не в SQL и не в JSON, а в модели:

**сериализация домена «в лоб» одной `Retrospection` на живой Quantum — это не проекция архива.**

Доменный тип обслуживает runtime (идентичности, references, umap по Pass, вложенные Technique с texture bindings). Архиву нужны другие границы: что входит в срез, что реконструируется после load, что никогда не пишется (GPU, runtimes). Когда форма = структура домена, любое изменение домена ломает архив *и* тянет за собой оба бэкенда.

Dual-write усугублял: два носителя должны были оставаться согласованными при том, что форма ещё не стабильна.

```text
Domain Quantum  ──Retrospection──►  Archive layout
       ▲                                    │
       │         (хотелось отождествить)    │
       └────────────────────────────────────┘

нужно было:
Domain ──► Projection(A) ──► JSON
      └──► Projection(B) ──► RDB   (или общий Projection, разные codecs)
```

Комментарий: отождествление «тип аспекта = форма архива» экономит строки на старте и дорого стоит, как только появляются nest, remap id, и «это в каталоге, а это нет».

## Решение сессии (заморозка, не выкидывание)

Стратегически:

1. **DB** — корректный снимок эксперимента; **не поддерживать**.
2. **JSON** — кандидат на будущий live-архив; сейчас тоже не рабочий контур.
3. **Toy / rmmr** — только hardcoded seed. `load`/`save` — заглушки.
4. **fQSM API** — `Retrospection` / `persistency.h` остаются частью фреймворка (тесты + будущий возврат). Домен **не обязан** специализировать.
5. **Эталон** — формы в `Etalon.fqsm/aspects.s11n.h` как музей; живой `aspects.q1.h` без s11n.

Формулировка итога:

> Работа по сериализации не выброшена — она выжила в юнит-тестах и архиве Эталона.  
> Но сериализация не рабочий инструмент проекта: из контура Toy/rmmr убрана, интерфейсы не мешают идти хардкодом.

```cpp
// после вырезки — рабочий путь
Manager::prepare(world, /*location ignored*/) {
    hardcodedInit(world);
    return PrepareStatus::Generated;
}

// fQSM по-прежнему знает про форму (framework surface)
using ::fqsm::aspect::Retrospection;   // api/interface.h
// домен rmmr больше не пишет specialization
```

## Выводы на возврат к RDB + JSON (+ binary)

1. **Сначала проекция, потом codec.** Форма архива — отдельный тип/схема среза, не «тот же Quantum». Codec (json / sqlite / binary) читает проекцию, не домен.

2. **Один бэкенд до стабилизации формы.** Dual-write имел смысл как проверка изоморфизма движков; как режим разработки домена — нет. Сначала один носитель (скорее JSON), RDB — когда layout проекции уже скучный.

3. **Срез каталога ≠ world.** Явно перечислять Meta в архиве; Group / Engine ownership / runtimes — снаружи, с правилами restore (extend группы, `materialize` после load).

4. **Nested relational — цена, не цель.** Дочерние таблицы и dotted columns работают, но закрепляют форму. Имеет смысл, когда проекция уже названа; иначе JSON (или blob + явные колонки-ключи) дешевле итераций.

5. **Remap id — часть контракта load**, не побочный эффект. Тесты (`remap_identities`, families) — держать как регрессию; в домен не тащить, пока нет проекции.

6. **Фреймворковый trait можно оставлять.** Пустой домен + живые тесты + архивный эталон — нормальная стоянка. Не путать «API фреймворка знает Retrospection» с «проект сохраняет ассеты через неё».

7. **Binary** (когда дойдёт): тот же урок — отдельная проекция + codec; не третий visitor поверх живого Quantum «потому что describe уже есть».

## Артефакты-якоря

| Что | Где | Статус |
|-----|-----|--------|
| Trait / `field`/`collection` | `fQSM/aspect/persistency.h` | framework, жив |
| Concept | `musthave::Retrospection` | жив |
| JSON form | `pQRF/json/value_form.h` | инфраструктура |
| RDB nest | `pQRF/database/form_tree.h`, `engine.h` | снимок, не maintain |
| Живые формы | fQSM tests (`community`, `temp_persistency`, …) | регрессия |
| Эталон | `Etalon.fqsm/aspects.s11n.h` | архив, без include |
| Домен rmmr / Toy | hardcoded | рабочий контур |

_Конец записи. Возврат к теме — с проекции, не с dual-write._
