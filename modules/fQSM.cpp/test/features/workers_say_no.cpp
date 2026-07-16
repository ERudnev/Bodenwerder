#include "_common.h"

#include <base/logging.h>
#include <fQSM/api/interface.h>

namespace {
    using namespace fqsm::api;

    namespace local {
        struct Cell : Entity<Cell> {
            struct Quantum {
                integer value;
            };
            struct Actions : BaseActions {
                static void halve(Writing context, Id id) {
                    if (get(context, id).value % 2 != 0)
                        context.deny("Cell::halve: odd value");
                    else
                        modify(context, id)->value /= 2;
                }
            };
            struct Internals : DefaultInternals{};
            static const Behavior customAspectReactions() { return {}; }
        };
    }
}

namespace tests {

void workers_say_no()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<Cell>(),
    });

    establish::Realm main(schema);

    const auto first = with<Cell>::create(main, {8});
    const auto second = with<Cell>::create(main, {6});

    main.branch([&](Writing context){
        with<Cell>::halve(context, first);
        with<Cell>::halve(context, second);
    });

    EXPECT_EQ(with<Cell>::get(main, first).value, 4);

    base::message("test: EXPECTED log message below: worker will refuse to work and provoke transaction to fail...");
    main.branch([&](Writing context){
        with<Cell>::halve(context, first);
        with<Cell>::halve(context, second);
    });
    base::message("...provoked message is above");

    EXPECT_EQ(with<Cell>::get(main, first).value, 4);
}

} // namespace tests
