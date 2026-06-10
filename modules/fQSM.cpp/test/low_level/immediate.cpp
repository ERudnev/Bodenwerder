#include "_common.h"

#include <fQSM/api/interface.h>
#include <fQSM/state/world/data.h>

namespace {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum {
            integer value;
        };
        static const Codex codex;
        struct Actions : BaseActions {
            static void fastJob(Immediate<A> context, int bonus) {
                for (auto& item : context.items)
                    item->value += bonus;
                if (context.items.size() > 3)
                    context.items[3] = context.items[2];
            }
        };
    };

    const A::Codex A::codex = {};
}

namespace tests {

void immediate()
{
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
    });

    fqsm::state::world::Data world(schema);
    context::Realm main(world);

    A::Actions::fastJob(main, 27);

}

} // namespace tests
