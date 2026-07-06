#include <fQSM/api/interface.h>

#include "placeholder.q1.h"

using namespace fqsm::api;

int main()
{
    base::message("workshop starts...");
    base::message("building Schema...");
    using namespace placeholder;
    static const Schema schema = ask::schema::merge({
        ask::schema::aspect<Minimal>(),
        ask::schema::aspect<Tag>(),
        ask::schema::aspect<MyAttribute>(),
    });
    base::message("...done Schema");

    context::Realm main(schema);

    base::message("creariing Bold...");
    const auto bold = with<Minimal>::create(main,{});
    base::message("...done Bold");

    base::message("creariing MyAttribute...");
    with<MyAttribute>::create_for(main, bold, {7, -7});
    base::message("...done MyAttribute");

    // just log
    with<MyAttribute>::justlog(main, bold);
    with<MyAttribute>::modify(main, bold)->x += 10;

    base::message("Bolds before experimental failure: {}", with<Minimal>::count(main));
    // provoke "demo" warning of "heawy" update (see Q1 MyAttribute::!localRule()
    {
        base::message("making too many Bolds at once (MyAttribute dont like such updates at once)");
        context::Branch branch(main);
        // 3 new Bold+MyAttribute will be too much for MyAttribute constraints
        // this will provoke transaction failure
        for (int xx = 0; xx < 3; ++xx) {
            const auto id = with<Minimal>::create(branch, {});
            with<MyAttribute>::create_for(branch, id, { -1 - xx, -1 - xx});
        }
    }
    base::message("Bolds after experimental failure: {}", with<Minimal>::count(main));

    // Call Kraken!
    base::message("Bold before Kraken: {}", with<Minimal>::exists(main, bold));
    with<Tag>::create_for(main, bold, {});
    with<Tag>::kraken(main, bold);
    base::message("Bold after Kraken: {}", with<Minimal>::exists(main, bold));

    base::message("deleting Bold which was already deleted...");
    with<Minimal>::remove(main, bold);
    base::message("...done unnecessary deleting");

}