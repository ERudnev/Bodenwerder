#include "_common.h"

#include <fQSM/api/interface.h>
#include <fQSM/state/world/data.h>

namespace {
    using namespace fqsm::api;
    namespace local {
        struct A : Entity<A> {
            struct Quantum { integer value; };
            static const Codex codex;
        };

        struct B : Component<B, A> {
            struct Quantum { string text; };
            static const Codex codex;
        };

        struct C : Component<C, A> {
            struct Quantum { integer power; };
            static const Codex codex;
            struct Capabilities : BaseCapabilities {
                static auto create(Writing, A::Id id, const A::Quantum& source) {
                };
            };
        };

    }

    // kinda impl in come *.cpp file:
    namespace local {
        const A::Codex A::codex = {};
    }
    namespace local {
        const B::Codex B::codex = {
            norma::component<B, A>(),
            norma::debug_death_event<B>("death-event message for {}"),
        };
    }
}

namespace tests {

void codex_and_indices()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
        ask::schema::aspect<B>(),
    });

    fqsm::state::world::Data world(schema);
    context::Realm main(world);

    const auto id = ask::item::create<A>(main, {4});
    EXPECT_EQ(ask::item::get<B>(main, id)->text, "generated");
}

} // namespace tests
