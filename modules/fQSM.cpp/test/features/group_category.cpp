#include "_common.h"

#include <fQSM/api/interface.h>

namespace {
namespace local {
    using namespace fqsm::api;

    struct Host : Entity<Host> {
        struct Quantum {};
        using Reactions = DefaultReactions;
    };

    struct Fragment : Entity<Fragment> {
        struct Quantum {};
        using Reactions = DefaultReactions;
    };

    struct Fragment_group : Group<Fragment_group, Host, Fragment> {};

    struct Manager : Archetype<Manager> {
        static Fragment::Id create(Writing context, Host::Id hostId) {
            const auto fragmentId = ask::item::create<Fragment>(context, {});
            ask::item::update<Fragment_group>(context, hostId)->insert(fragmentId);
            return fragmentId;
        }
    };
}
} // namespace

namespace tests {

void group_category()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<Host>(),
        ask::schema::aspect<Fragment>(),
        ask::schema::aspect<Fragment_group>(),
    });

    context::Realm main(schema);

    const auto hostId = ask::item::create<Host>(main, {});
    ask::item::create<Fragment_group>(main, hostId, {});

    with<Manager>::create(main, hostId);
}

} // namespace tests
