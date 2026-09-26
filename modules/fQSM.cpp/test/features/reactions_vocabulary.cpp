#include "_common.h"

#include <fQSM/api/interface.h>

// Reaction builders that no other test drives: constraint::element_wide and debug::death_log.
namespace {
namespace local {
    using namespace fqsm::api;

    struct Gauge : Entity<Gauge> {
        struct Quantum { integer value; };
        struct Global { integer cap = 10; };
        struct Internals : DefaultInternals {
            // contextual: the cap comes from the proposal, the Reacting context arrives as Reading
            static auto clamp(Reading context, Id, const Quantum& quantum) -> PossibleChange {
                const auto cap = with<Gauge>::get_global(context).cap;
                if (quantum.value <= cap) return std::nullopt;
                return Quantum{cap};
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::constraint::element_wide<Gauge>(&Internals::clamp),
                reaction::debug::death_log<Gauge>("gauge changed"),
            };
        }
    };
}
} // namespace

namespace tests {

void reactions_vocabulary()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::aspect<Gauge>();
    establish::Realm main(schema);

    const auto low = with<Gauge>::create(main, {5});
    EXPECT_TRUE(main.result().good());
    EXPECT_FALSE(main.result().warning.empty()) << "death_log warns and does not refuse";
    EXPECT_EQ(with<Gauge>::get(main, low).value, 5);

    const auto high = with<Gauge>::create(main, {50});
    EXPECT_EQ(with<Gauge>::get(main, high).value, 10) << "element_wide corrects the new value";

    main.branch([&](Writing context) {
        with<Gauge>::modify_global(context)->cap = 3;
        with<Gauge>::modify(context, low)->value = 4;
    });
    EXPECT_EQ(with<Gauge>::get(main, low).value, 3) << "the correction reads the proposal (new cap)";
    EXPECT_EQ(with<Gauge>::get(main, high).value, 10) << "an unchanged item is not re-evaluated";
}

} // namespace tests
