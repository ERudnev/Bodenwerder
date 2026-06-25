#include "_common.h"

#include <fQSM/api/interface.h>

using namespace fqsm::api;
namespace local {

    struct EntFree : Entity<EntFree> {
        struct Quantum { integer value; };
        static const Behavior behavior; // TODO: struct Behavoir : BaseReactions { static const Rules rules; }
    };

    struct CompSimple : Component<CompSimple, EntFree> {
        struct Quantum { string text; };
        static const Behavior behavior;
    };

    struct CompWithCreate : Component<CompWithCreate, EntFree> {
        struct Quantum { integer square; };
        static const Behavior behavior;
        struct Actions : BaseActions {
        private:
            static int square(int x) { return x*x; };
        public:
            static void create(Writing context, EntFree::Id id, bool askPowerOf4) {
                int x = square(ask::item::get<EntFree>(context, id)->value);
                if (askPowerOf4) x = square(x);
                ask::item::create<CompWithCreate>(context, id, {x});
            };
        };
    };

    struct AttrPrimary : Attribute<AttrPrimary, EntFree> {
        struct Quantum { string description; };
        static const Behavior behavior;
    };

    struct AttrSecondary : Attribute<AttrSecondary, AttrPrimary> {
        struct Quantum { string clarification; };
        static const Behavior behavior;
    };

    namespace Archetypes {

        // simulates decomposition, where CompSimple~CompWithCreate and it is not fair to choose one as "main thing", ABC is "above"
        struct EntTwoComps : Archetype {
            static EntFree::Id spawn_correct(Writing context, int val) {
                const auto id = ask::item::create<EntFree>(context, {val});
                ask::item::create<CompSimple>(context, id, {std::format("it is {}", val)});
                with<CompWithCreate>::create(context, id, true);
                return id;
            }

            static EntFree::Id spawn_forgot_init_one_comp(Writing context, int val) {
                const auto id = ask::item::create<EntFree>(context, {val});
                // two comps are forgot to init. CompWithCreate passes this, but CompNoDefault kill entire Aggregate
                // end! for testing purposes
                return id;
            }
        };
    }
}

// kinda impl in come *.cpp file:
namespace local {
    const EntFree::Behavior EntFree::behavior = {};
}
namespace local {
    const CompSimple::Behavior CompSimple::behavior = {
        rule::structural::remove_with_parent<CompSimple,EntFree>(),
        rule::structural::dead_component_kill_parent<CompSimple, EntFree>(),
        rule::structural::parastic_requires_parent_to_appear<CompSimple, EntFree>(),
        rule::structural::parrent_appears_requires_component<CompSimple, EntFree>(),
        //rule::structural_deprecated::component<CompSimple, EntFree>(reflex::ComponentMissing::inacceptable),
        reaction::debug::death_log<CompSimple>("death-event message for {}"),
    };
}
namespace local {
    const CompWithCreate::Behavior CompWithCreate::behavior = {
        rule::structural::remove_with_parent<CompWithCreate,EntFree>(),
        rule::structural::dead_component_kill_parent<CompWithCreate, EntFree>(),
        rule::structural::parastic_requires_parent_to_appear<CompWithCreate, EntFree>(),
        rule::structural::parrent_appears_requires_component<CompWithCreate, EntFree>(),
        //rule::structural_deprecated::component<CompWithCreate, EntFree>(reflex::ComponentMissing::remove_parent, &CompWithCreate::Actions::create),
        reaction::debug::death_log<CompWithCreate>("death-event message for {}"),
    };
}

namespace local {
    const AttrPrimary::Behavior AttrPrimary::behavior = {
        rule::structural::remove_with_parent<AttrPrimary, EntFree>(),
        rule::structural::parastic_requires_parent_to_appear<AttrPrimary, EntFree>(),
    };
}

namespace local {
    const AttrSecondary::Behavior AttrSecondary::behavior = {
        rule::structural::remove_with_parent<AttrSecondary, AttrPrimary>(),
        rule::structural::parastic_requires_parent_to_appear<AttrSecondary, AttrPrimary>(),
    };
}


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
        context::Realm main(world);
        const auto id = Archetypes::EntTwoComps::spawn_correct(main, 1);
        EXPECT_EQ(fqsm::Reading(main).quanta(), 3) << "all 3 items created, verified and part of realm now";
    }

    {
        context::Realm main(world);
        const auto id = Archetypes::EntTwoComps::spawn_correct(main, 1);
        const auto storedVal = ask::item::get<EntFree>(main, id)->value;
        ask::item::update<EntFree>(main, id).remove();
        EXPECT_EQ(fqsm::Reading(main).quanta(), 0) << "normalization killed everything by parent remove";
        ask::item::create<CompSimple>(main, id, {std::format("i am sorry, i am late", storedVal)});
        EXPECT_EQ(fqsm::Reading(main).quanta(), 0) << "normalization killed ill-formed componet";
    }

    {
        context::Realm main(world);
        const auto id = Archetypes::EntTwoComps::spawn_forgot_init_one_comp(main, 1);
        EXPECT_EQ(fqsm::Reading(main).quanta(), 0) << "invalid implementation (forgot component) of spawn succesfully detected";
    }



    {
        context::Realm main(world);
        const auto id = Archetypes::EntTwoComps::spawn_correct(main, 1);
        with<CompSimple>::kill(main, id);
        EXPECT_EQ(fqsm::Reading(main).quanta(), 0) << "composite killed by component";
    }

}

} // namespace tests
