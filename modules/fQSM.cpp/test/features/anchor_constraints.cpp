#include "_common.h"

#include <fQSM/api/interface.h>

namespace local {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum {};
        using Reactions = DefaultReactions;
    };

    struct B : Entity<B> {
        struct Quantum {
            Anchor<A> iNeedThis;
            AnchorOpt<A> iNeedThisOnlyNow;
            Control<A> controlledOther;
        };

        struct Reactions : BaseReactions {
            static const Behavior custom;
        };
    };

    const B::Reactions::Behavior B::Reactions::custom = {
        reaction::structural::anchored<B, A, &B::Quantum::iNeedThis>{},
        reaction::structural::anchored_optional<B, A, &B::Quantum::iNeedThisOnlyNow>{},
        reaction::structural::controls<B, A, &B::Quantum::controlledOther>{},
    };
}

namespace tests {

void anchor_constraints()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
        ask::schema::aspect<B>(),
    });

    context::Realm main(schema);

    const auto a1 = ask::item::create<A>(main, {});
    const auto a2 = ask::item::create<A>(main, {});
    const auto a1dummy = ask::item::create<A>(main, {});
    const auto a2dummy = ask::item::create<A>(main, {});

    const auto b1 = ask::item::create<B>(main, {.iNeedThis = a1, .iNeedThisOnlyNow{}, .controlledOther = a1dummy});
    const auto b2 = ask::item::create<B>(main, {.iNeedThis = a1, .iNeedThisOnlyNow{a2}, .controlledOther = a2dummy});

    ask::item::update<A>(main, a2).remove();
    EXPECT_TRUE(ask::item::exists<B>(main, b1)) << "b1 must survive removal of a2";
    EXPECT_FALSE(ask::item::exists<B>(main, b2)) << "b2 removed by optional anchor";
    EXPECT_FALSE(ask::item::exists<A>(main, a2dummy)) << "a2dummy removed with b2";

    ask::item::update<A>(main, a1).remove();
    EXPECT_FALSE(ask::item::exists<B>(main, b1)) << "b1 must die with a1";
}

} // namespace tests
