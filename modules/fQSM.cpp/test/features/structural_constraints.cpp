#include "_common.h"

#include <fQSM/api/interface.h>

namespace {
using namespace fqsm::api;

namespace local {

    struct EntFree : Entity<EntFree> {
        struct Quantum { integer value; };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct CompSimple : Component<CompSimple, EntFree> {
        struct Quantum { string text; };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() {
            return {
                //rule::structural_deprecated::component<CompSimple, EntFree>(reflex::ComponentMissing::inacceptable),
                reaction::debug::death_log<CompSimple>("death-event message for {}"),
            };
        }
    };

    struct CompWithCreate : Component<CompWithCreate, EntFree> {
        struct Quantum { integer square; };

        struct Actions : BaseActions {
        private:
            static int square(int x) { return x*x; };
        public:
            static void create(Writing context, EntFree::Id id, bool askPowerOf4) {
                int x = square(with<EntFree>::get(context, id).value);
                if (askPowerOf4) x = square(x);
                extend(context, id, {x});
            };
        };

        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() {
            return {
                reaction::debug::death_log<CompWithCreate>("death-event message for {}"),
            };
        }
    };

    struct AttrPrimary : Attribute<AttrPrimary, EntFree> {
        struct Quantum { string description; };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct AttrSecondary : Attribute<AttrSecondary, AttrPrimary> {
        struct Quantum { string clarification; };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    namespace archetype {

        // simulates decomposition, where CompSimple~CompWithCreate and it is not fair to choose one as "main thing", ABC is "above"
        struct EntTwoComps : Archetype<EntTwoComps> {
            static EntFree::Id spawn_correct(Writing context, int val) {
                const auto id = with<EntFree>::create(context, {val});
                with<CompSimple>::extend(context, id, {std::format("it is {}", val)});
                with<CompWithCreate>::create(context, id, true);
                return id;
            }

            static EntFree::Id spawn_forgot_init_one_comp(Writing context, int val) {
                const auto id = with<EntFree>::create(context, {val});
                // two comps are forgot to init. CompWithCreate passes this, but CompNoDefault kraken entire Aggregate
                // end! for testing purposes
                return id;
            }
        };
    }
}
} // namespace

namespace tests {

void structural_constraints()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<EntFree>(),
        ask::schema::aspect<CompSimple>(),
        ask::schema::aspect<CompWithCreate>(),
    });

    fqsm::model::complex::Reality world(schema);

    {
        establish::Realm main(world);
        const auto id = with<archetype::EntTwoComps>::spawn_correct(main, 1);
        EXPECT_EQ(fqsm::Reading(main)->quanta(), 3) << "all 3 items created, verified and part of realm now";
    }

    {
        establish::Realm main(world);
        const auto id = with<archetype::EntTwoComps>::spawn_correct(main, 1);
        const auto storedVal = with<EntFree>::get(main, id).value;
        with<EntFree>::remove(main, id);
        EXPECT_EQ(fqsm::Reading(main)->quanta(), 0) << "normalization killed everything by parent remove";
        with<CompSimple>::extend(main.silent_work(), id, {std::format("i am sorry, i am late", storedVal)});
        EXPECT_EQ(fqsm::Reading(main)->quanta(), 0) << "normalization killed ill-formed component";
    }

    {
        establish::Realm main(world);
        const auto attempted = with<EntFree>::create(main.silent_work(), {42});
        EXPECT_FALSE(with<EntFree>::exists(main, attempted)) << "bare EntFree must not survive structural normalization";
        const auto id = with<archetype::EntTwoComps>::spawn_forgot_init_one_comp(main.silent_work(), 1);
        EXPECT_EQ(fqsm::Reading(main)->quanta(), 0) << "invalid implementation (forgot component) of spawn succesfully detected";
    }

    {
        establish::Realm main(world);
        const auto id = with<archetype::EntTwoComps>::spawn_correct(main, 1);
        with<CompSimple>::kraken(main, id);
        EXPECT_EQ(fqsm::Reading(main)->quanta(), 0) << "composite killed by component";
    }

}

} // namespace tests
