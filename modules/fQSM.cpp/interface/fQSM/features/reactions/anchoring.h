#pragma once

#include <optional>
#include <set>
#include <vector>

#include <base/logging.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>
namespace fqsm::features::reactions::structural::details {

    template<typename Client, typename Observed>
    using LinkValue = Id<Observed> Quantum<Client>::*;

    template<typename Client, typename Observed>
    using OptionalLinkValue = std::optional<Id<Observed>> Quantum<Client>::*;

    template<typename Client, typename Observed>
    using LinkVector = std::vector<Id<Observed>> Quantum<Client>::*;

    template<typename Client, typename Observed>
    using LinkSet = std::set<Id<Observed>> Quantum<Client>::*;

}

namespace fqsm::features::reactions::structural {

    template<category::Any Client, category::Any Observed, details::LinkValue<Client, Observed> link>
    struct anchored;

    template<category::Any Client, category::Any Observed, details::OptionalLinkValue<Client, Observed> link>
    struct anchored_optional;

    template<category::Any Client, category::Any Observed, details::LinkValue<Client, Observed> link>
    struct controls;

    template<category::Any Client, category::Any Observed, details::LinkVector<Client, Observed> link>
    struct anchored_all;

    template<category::Any Client, category::Any Observed, details::LinkSet<Client, Observed> link>
    struct anchored_any;

    template<category::Any Client, category::Any Observed, details::LinkVector<Client, Observed> link>
    struct controls_all;

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

    template<category::Any Client, category::Any Observed, details::OptionalLinkValue<Client, Observed> link>
    struct anchored_optional final : Abstract {
        Sources listens() const override { return typed_set<Observed>(); }

        void apply(Reacting context) override {
            auto& clientPatch = context.reaction<Client>();
            for (const auto& change : changes<Observed>(context).removed()) {
                for (const auto& entry : context.proposal.aspect<Client>().items()) {
                    const auto& slot = entry.value.*link;
                    if (slot.has_value() && *slot == change.id)
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

    // _INCOMPLETE_: Client anchored to every Observed::Id in link vector.
    template<category::Any Client, category::Any Observed, details::LinkVector<Client, Observed> link>
    struct anchored_all final : Abstract {
        Sources listens() const override { return typed_set<Observed>(); }
        void apply(Reacting) override { _INCOMPLETE_; }
    };

    // _INCOMPLETE_: Client anchored to any Observed::Id from link set.
    template<category::Any Client, category::Any Observed, details::LinkSet<Client, Observed> link>
    struct anchored_any final : Abstract {
        Sources listens() const override { return typed_set<Observed>(); }
        void apply(Reacting) override { _INCOMPLETE_; }
    };

    // _INCOMPLETE_: Client owns every Observed::Id in link vector.
    template<category::Any Client, category::Any Observed, details::LinkVector<Client, Observed> link>
    struct controls_all final : Abstract {
        Sources listens() const override { return typed_set<Client>(); }
        void apply(Reacting) override { _INCOMPLETE_; }
    };

}
