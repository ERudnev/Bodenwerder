#include <fQSM/api/interface.h>

#include "placeholder.q1.h"

using namespace fqsm::api;

int main()
{
    base::message("workshop starts...");
    using namespace placeholder;
    static const Schema schema = ask::schema::merge({
        ask::schema::aspect<Minimal>(),
        ask::schema::aspect<Tag>(),
        ask::schema::aspect<MyAttribute>(),
    });

    establish::Realm main(schema);

    const auto bold = with<Minimal>::create(main,{});
    with<MyAttribute>::extend(main, bold, {7, -7});

    // just log
    with<MyAttribute>::justlog(main, bold);
    with<MyAttribute>::modify(main, bold)->x += 10;

    // experimental:
    const auto id = main.branch([](fqsm::Writing context){
        const auto id = with<Minimal>::create(context, {});
        with<MyAttribute>::extend(context, id, {-100, -100});
        return id;
    });

    // provoke "demo" warning of "heawy" update (see Q1 MyAttribute::!localRule()
    main.branch([](fqsm::Writing context) {
        // 3 new Bold+MyAttribute will be too much for MyAttribute constraints
        // this will provoke transaction failure
        for (int xx = 0; xx < 3; ++xx) {
            const auto id = with<Minimal>::create(context, {});
            with<MyAttribute>::extend(context, id, { -1 - xx, -1 - xx});
        }
    });

    // Call Kraken!
    with<Tag>::extend(main, bold, {});
    with<Tag>::kraken(main, bold);

    with<Minimal>::remove(main, bold);

}