#include "_common.h"

#include <fQSM/api/interface.h>

namespace {
namespace local {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum {};
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct B : Entity<B> {
        struct Quantum {
            Anchor<A> iNeedThis;
            Control<A> controlledOther;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() {
            return {
                reaction::structural::anchored<B, A, &B::Quantum::iNeedThis>{},
                reaction::structural::controls<B, A, &B::Quantum::controlledOther>{},
            };
        }
    };
}
} // namespace

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

    const auto a1 = with<A>::create(main, {});
    const auto a2 = with<A>::create(main, {});
    const auto a1dummy = with<A>::create(main, {});

    const auto b1 = with<B>::create(main, {.iNeedThis = a1, .controlledOther = a1dummy});

    with<A>::remove(main, a2);
    EXPECT_TRUE(with<B>::exists(main, b1)) << "b1 must survive removal of unrelated a2";
    EXPECT_TRUE(with<A>::exists(main, a1dummy));

    with<B>::remove(main, b1);
    EXPECT_FALSE(with<A>::exists(main, a1dummy)) << "controlledOther removed with b1";

    const auto a1dummy2 = with<A>::create(main, {});
    const auto b2 = with<B>::create(main, {.iNeedThis = a1, .controlledOther = a1dummy2});
    with<A>::remove(main, a1);
    EXPECT_FALSE(with<B>::exists(main, b2)) << "b2 must die with anchored a1";
}

} // namespace tests
