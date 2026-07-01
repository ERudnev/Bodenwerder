#include "_common.h"

#include <fQSM/api/interface.h>
#include <fQSM/features/reaction.h>

namespace fqsm::workshop_temp {

    template<typename Client, typename Observed>
    using LinkValue = Id<Observed> Quantum<Client>::*;

    // Draft for future features::reactions::anchor::remove_when_observed_dies<Client, Observed, link>.
    template<category::Any Client, category::Any Observed, LinkValue<Client, Observed> link>
    struct remove_when_host_removed final : features::reactions::Abstract {
        Sources listens() const override { return typed_set<Observed>(); }

        void apply(Reacting context) override {
            auto& clientPatch = context.reaction<Client>();
            for (const auto& change : changes<Observed>(context).removed()) {
                for (const auto& entry : context.proposal.aspect<Client>().items()) {
                    if ((entry.value.*link) == change.id)
                        clientPatch.put_deletion(entry.id);
                }
            }
        }
    };

}

namespace local {
    using namespace fqsm::api;
    using namespace fqsm::workshop_temp;

    struct A : Entity<A> {
        struct Quantum {};
        using Reactions = DefaultReactions;
    };

    struct B : Entity<B> {
        struct Quantum {
            A::Id anchor;
        };

        struct Reactions : BaseReactions {
            static const Behavior custom;
        };
    };

    const B::Reactions::Behavior B::Reactions::custom = {
        remove_when_host_removed<B, A, &B::Quantum::anchor>{},
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

    const auto aId = ask::item::create<A>(main, {});
    const auto bId = ask::item::create<B>(main, {.anchor = aId});

    ask::item::update<A>(main, aId).remove();

    EXPECT_FALSE(ask::item::exists<B>(main, bId)) << "B must die when anchored A is removed";
}

} // namespace tests
