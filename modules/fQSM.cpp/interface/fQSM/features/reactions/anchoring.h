#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>

namespace fqsm::features::reactions::structural::details {

    template<typename Client, typename Observed>
    using LinkValue = Id<Observed> Quantum<Client>::*;

}

namespace fqsm::features::reactions::structural {

    template<category::Any Client, category::Any Observed, details::LinkValue<Client, Observed> link>
    struct anchored;

    template<category::Any Client, category::Any Observed, details::LinkValue<Client, Observed> link>
    struct controls;

    template<category::Any Client, category::Any Observed, details::LinkValue<Client, Observed> link>
    struct management;

}

// Impl:
namespace fqsm::features::reactions::structural {

    template<category::Any Client, category::Any Observed, details::LinkValue<Client, Observed> link>
    struct anchored final : Abstract {
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

    // Client owns Observed at link: when Client is removed, remove linked Observed.
    template<category::Any Client, category::Any Observed, details::LinkValue<Client, Observed> link>
    struct controls final : Abstract {
        Sources listens() const override { return typed_set<Client>(); }

        void apply(Reacting context) override {
            auto& observedPatch = context.reaction<Observed>();
            for (const auto& change : changes<Client>(context).removed())
                observedPatch.put_deletion(change.throwing_before().*link);
        }
    };

}
