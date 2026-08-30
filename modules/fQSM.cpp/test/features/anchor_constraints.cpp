#include "_common.h"

#include <fQSM/api/interface.h>

namespace {
namespace local {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum {
            string name;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct B : Entity<B> {
        struct Quantum {
            Anchor<A> iNeedThis;
            Custody<A> custodyOther;
        };
        struct Actions : BaseActions {
            static auto nameOfOther(Reading context, Id id)->string {
                const auto other = ward(context, id, &Quantum::custodyOther);
                return other ? other->name : "";
            }
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() {
            return {
                reaction::structural::anchored<B, A, &B::Quantum::iNeedThis>{},
                reaction::structural::custody<B, A, &B::Quantum::custodyOther>{},
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

    establish::Realm main(schema);

    const auto a1 = with<A>::create(main, {"first"});
    const auto a2 = with<A>::create(main, {"second"});
    const auto a1dummy = with<A>::create(main, {"dummy"});

    const auto b1 = with<B>::create(main, {.iNeedThis = a1, .custodyOther = a1dummy});
    const auto bShare = with<B>::create(main, {.iNeedThis = a1, .custodyOther = a1dummy});

    with<A>::remove(main, a2);
    EXPECT_TRUE(with<B>::exists(main, b1)) << "b1 must survive removal of unrelated a2";
    EXPECT_TRUE(with<A>::exists(main, a1dummy));

    EXPECT_EQ(with<B>::nameOfOther(main, bShare), "dummy");
    with<B>::remove(main, b1);
    EXPECT_FALSE(with<A>::exists(main, a1dummy)) << "custodyOther removed with b1";
    EXPECT_FALSE(with<B>::exists(main, bShare)) << "other holder of the same ward dies with it";

    const auto a1dummy2 = with<A>::create(main, {"dummy2"});
    const auto b2 = with<B>::create(main, {.iNeedThis = a1, .custodyOther = a1dummy2});
    with<A>::remove(main, a1);
    EXPECT_FALSE(with<B>::exists(main, b2)) << "b2 must die with anchored a1";

    const auto aKeep = with<A>::create(main, {"keep"});
    const auto aWard = with<A>::create(main, {"ward"});
    const auto b3 = with<B>::create(main, {.iNeedThis = aKeep, .custodyOther = aWard});
    with<A>::remove(main, aWard);
    EXPECT_FALSE(with<B>::exists(main, b3)) << "holder dies when custody ward is murdered";
}

} // namespace tests
