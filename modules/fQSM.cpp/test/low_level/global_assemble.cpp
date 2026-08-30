#include "_common.h"

#include <fQSM/api/interface.h>

#include <stdexcept>

namespace {
using namespace fqsm::api;

namespace model {

    struct Host : Entity<Host> {
        struct Quantum {};
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Seed : Entity<Seed> {
        struct Quantum {};
        struct Global {
            Host::Id host;
        };
        struct Always {
            static auto assemble(SettingUp& setup) -> Global {
                auto world = setup.writing();
                const auto id = with<Host>::create(world, {});
                return Global{.host = id};
            }
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
} // namespace

namespace tests {

void global_assemble()
{
    using namespace model;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<Host>(),
        ask::schema::aspect<Seed>(),
    });

    establish::Realm main(schema);
    const auto host = with<Seed>::get_global(main).host;
    EXPECT_TRUE(with<Host>::exists(main, host));

    bool refused = false;
    try {
        establish::Realm missing(ask::schema::aspect<Seed>());
    } catch (const std::exception&) {
        refused = true;
    }
    EXPECT_TRUE(refused) << "Seed.assemble without Host in schema must fail world birth";
}

} // namespace tests
