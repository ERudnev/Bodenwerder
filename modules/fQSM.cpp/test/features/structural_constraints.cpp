#include "_common.h"

#include <fQSM/api/interface.h>

using namespace fqsm::api;
namespace local {
    struct A : Entity<A> {
        struct Quantum { integer value; };
        static const Behavior behavior; // TODO: struct Behavoir : BaseReactions { static const Rules rules; }
    };

    struct B : Component<B, A> {
        struct Quantum { string text; };
        static const Behavior behavior;
    };

    struct C : Attribute<C, A> {
        struct Quantum { integer square; };
        static const Behavior behavior;
        struct Actions : BaseActions {
            static void create(Writing context, A::Id id) {
                //ask::item::create<C>(context, id, {with<A>::get(context, id).value});
                ask::item::create<C>(context, id, {7});
            }
        };
    };

    struct Archetype {
        static A::Id spawn(fqsm::Writing context, int val) {
            base::message("spawn A");
            const auto id = ask::item::create<A>(context, {val});
            base::message("spawn B");
            ask::item::create<B>(context, id, {std::format("it is {}", val)});
            base::message("spawn C");
            with<C>::create(context, id);
            return id;
        }
    };
}

// kinda impl in come *.cpp file:
namespace local {
    const A::Behavior A::behavior = {};
}
namespace local {
    const B::Behavior B::behavior = {
        rule::structural_deprecated::component<B, A>(reflex::ComponentMissing::inacceptable),
        reaction::debug::death_log<B>("death-event message for {}"),
    };
}
namespace local {
    const C::Behavior C::behavior = {
        rule::structural_deprecated::component<C, A>(reflex::ComponentMissing::remove_parent, &C::Actions::create),
        reaction::debug::death_log<C>("death-event message for {}"),
    };
}


namespace tests {

void structural_constraints()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
        ask::schema::aspect<B>(),
        ask::schema::aspect<C>(),
    });

    fqsm::model::complex::Reality world(schema);
    context::Realm main(world);
    const auto id = local::Archetype::spawn(main, 1);
    base::message("removing A");
    ask::item::update<A>(main, id).remove();

}

} // namespace tests
