# Transaction lambda

**upd: 2026-07-14**
Прибил как назойливую муху
```cpp
    const auto id = main.branch([](Writing context) {
        const auto id = with<Entity>::create(context, {});
        with<Component>::extend(context, a, {});
        with<Feature>::extend(context, a, {});
        return id;
    });

```

2026-07-13

Хочется, чтобы у `Realm` появилась совсем простая и выразительная форма локальной транзакции:

```cpp
const val result = (Transaction)Realm::transaction(lambda->val);
```

Смысл в том, чтобы временный branch/scoped transaction перестал быть ритуалом руками и стал обычной операцией языка модели: открыл транзакцию, выполнил несколько связанных действий, получил результат, схлопнул patch.

Сейчас это выглядит так:

```cpp
A::Id id{};
{
    establish::Branch local(main);
    id = with<A>::create(local, {.value = 42});
    with<B>::extend(local, id, {});
}
```

Это пока рабочее, но еще не "очень". Хочется, чтобы связанный конгломерат действий рождался в одной транзакции без лишнего шумового кода и без ручного контроля времени жизни branch.

Возможные формы, которые хочется покрутить рядом:

```cpp
const auto id = main.transaction([](auto tx) {
    const auto id = with<A>::create(tx, {.value = 42});
    with<B>::extend(tx, id, {});
    return id;
});
```


```cpp
const auto id = main.branch([](auto local) {
    const auto id = with<A>::create(local, {.value = 42});
    with<B>::extend(local, id, {});
    return id;
});
```

У всех одна и та же мечта: branch остается внутренней механикой, а снаружи видна уже не техника scoped lifetime, а намерение "выполни это как одну транзакцию и верни результат".
