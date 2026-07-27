#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>
#include <fQSM/manipulation/relations.h>

namespace fqsm::features::reactions::structural::details {

    template<typename Client, typename Observed>
    using LinkValue = Id<Observed> Quantum<Client>::*;

}

namespace fqsm::features::reactions::structural {

    template<category::Any Client, category::Any Observed, details::LinkValue<Client, Observed> link>
    struct anchored;

    template<category::Any Client, category::Any Observed, details::LinkValue<Client, Observed> link>
    struct custody;

    template<category::Any Client, category::Any Observed, details::LinkValue<Client, Observed> link>
    struct management;

}

// Impl:
namespace fqsm::features::reactions::structural {

    template<category::Any Client, category::Any Observed, details::LinkValue<Client, Observed> link>
    struct anchored final : Abstract {
        Sources listens() const override { return typed_set<Observed>(); }

        void apply(Reacting context) override {
            auto& clientPatch = context.adjustments<Client>();
            const auto holders = ask::relations<Observed>(context).template removed<Client, link>();
            for (const auto& change : changes<Observed>(context).removed()) {
                for (const auto id : holders.ids(change.id))
                    clientPatch.put_deletion(id);
            }
        }
    };

    // Local cleanup custody at link: when Client is removed, request deletion of linked Observed.
    // No uniqueness/liveness warranty; external death of Observed does not touch this field.
    template<category::Any Client, category::Any Observed, details::LinkValue<Client, Observed> link>
    struct custody final : Abstract {
        Sources listens() const override { return typed_set<Client>(); }

        void apply(Reacting context) override {
            auto& observedPatch = context.adjustments<Observed>();
            for (const auto& change : changes<Client>(context).removed())
                observedPatch.put_deletion(change.old.*link);
        }
    };

}
