#pragma once

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>
#include <memory>

#include <iQSM/world.h>
#include <iQSM/delta.h>
#include <iQSM/helpers/particle.h>
#include <iQSM/meta/operationSemantics.h>
#include <iQSM/types.h>
#include <iQSM/repository/permit.h>
#include <iQSM/repository/transactions/staged.h>



namespace iqsm::operations::validation {

    namespace helpers {
        // Applies Func to each (id, item) in Field<Meta> and stages updates:
        // - Func(World, Id<Meta>, const Quantum<Meta>&) -> std::optional<Quantum<Meta>>
        // - if Func returns {}, no change
        // - otherwise updates item
        template<meta::Aspect Meta, auto Func>
        void for_each_item(Writing writing);
    }

    namespace structural {
        template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
        void anchor(Writing);

        template<meta::Aspect Anchor, meta::Aspect Dependee>
        void anchor_attribute(Writing); // confinement

        template<meta::Aspect Anchor, meta::Aspect Dependee, auto Construct>
        void isomorphic(Writing); // 1-1 iff confinement

        // Like anchor(), but the member is optional-like:
        // - if optional has no value -> ignored
        // - if optional has value and Anchor is missing -> Dependee is removed
        template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
        void anchor_optional(Writing);

        template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
        void anchor_any(Writing);

        // Member: std::vector<Anchor::Id> (every id must exist) or std::pair<Anchor::Id, Anchor::Id> (both must exist)
        template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
        void anchor_all(Writing);
    }

    namespace logic {
        // Ensures Dependee existence from Anchor by predicate.
        template<meta::Aspect Dependee, meta::Aspect Anchor, auto Need>
        void existence(Writing);

        // Like existence(), but uses Construct to build new Dependee items when needed.
        // Construct signature: void(Writing, AnchorId, Item<Anchor>) — writes via the same mandate (e.g. particle::create).
        template<meta::Aspect Dependee, meta::Aspect Anchor, auto Need, auto Construct>
        void existence(Writing);
    }
} // namespace iqsm::operations::validation

namespace iqsm::detail::operations::validation {
    template<meta::Aspect Anchor, meta::Aspect Dependee, typename Extract>
    void anchor_impl(Writing writing, Extract extract)
    {
        repo::Staged staged(writing);

        const auto anchor_field = staged->field<Anchor>();
        const auto dependee_field = staged->field<Dependee>();

        for (const auto& kv : dependee_field->container) {
            const auto& dependee_id = kv.first;
            const auto& dependee_item = kv.second;

            const auto anchor_id = extract(dependee_id, dependee_item);
            if (not anchor_field->container.contains(anchor_id)) {
                staged.remove<Dependee>(dependee_id, dependee_item);
            }
        }
    }
} // namespace iqsm::detail::operations::validation

namespace iqsm::operations::validation {
    template<meta::Aspect Meta, auto Func>
    void helpers::for_each_item(Writing writing) {
        using Id = ::iqsm::Id<Meta>;
        using Quantum = ::iqsm::Quantum<Meta>;
        using Item = ::iqsm::Item<Meta>;

        static_assert(meta::operations::ForEachItemUpdate<Meta, decltype(Func)>);

        repo::Staged staged{writing};
        const auto field = staged->field<Meta>();

        for (const auto& kv : field->container) {
            const auto& id = kv.first;
            const Item& before = kv.second;

            const auto after = std::invoke(Func, staged, id, *before);
            if (not after.has_value()) continue;
            staged.update<Meta>(id, before, base::make_shared<const Quantum>(std::move(*after)));
        }
    }

    template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
    void structural::anchor(Writing writing)
    {
        static_assert(std::is_member_object_pointer_v<decltype(Member)>);

        using Quantum = iqsm::Quantum<Dependee>;
        using AnchorId = Id<Anchor>;
        using MemberValue = decltype(std::declval<Quantum>().*Member);
        static_assert(std::is_convertible_v<MemberValue, AnchorId>);

        return detail::operations::validation::anchor_impl<Anchor, Dependee>(writing, [](auto, auto dependee_item) -> AnchorId {
            return static_cast<AnchorId>(dependee_item.get()->*Member);
        });
    }

    template<meta::Aspect Anchor, meta::Aspect Dependee>
    void structural::anchor_attribute(Writing writing)
    {
        return detail::operations::validation::anchor_impl<Anchor, Dependee>(writing, [](auto dependee_id, auto) { return dependee_id; });
    }

    template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
    void structural::anchor_optional(Writing writing)
    {
        static_assert(std::is_member_object_pointer_v<decltype(Member)>);

        repo::Staged staged{writing};

        using Quantum = iqsm::Quantum<Dependee>;
        using AnchorId = Id<Anchor>;
        using MemberValue = std::remove_reference_t<decltype(std::declval<Quantum&>().*Member)>;

        static_assert(
            requires(const MemberValue& v) { v.has_value(); *v; },
            "anchor_optional: Member must be an optional-like type (has_value() and operator*)"
        );
        static_assert(
            std::is_convertible_v<decltype(*std::declval<const MemberValue&>()), AnchorId>,
            "anchor_optional: optional value must be convertible to Anchor::Id"
        );

        const auto anchor_field = staged->field<Anchor>();
        const auto dependee_field = staged->field<Dependee>();

        for (const auto& kv : dependee_field->container) {
            const auto& dependee_id = kv.first;
            const auto& dependee_item = kv.second;

            const auto& opt = (dependee_item.get()->*Member);
            if (not opt.has_value()) continue;

            const auto anchor_id = static_cast<AnchorId>(*opt);
            if (not anchor_field->container.contains(anchor_id)) {
                staged.remove<Dependee>(dependee_id, dependee_item);
            }
        }
    }

    template<meta::Aspect Dependee, meta::Aspect Anchor, auto Need>
    void logic::existence(Writing writing)
    {
        repo::Staged staged{writing};

        using AnchorId = Id<Anchor>;
        using AnchorItem = iqsm::Item<Anchor>;
        using DependeeQuantum = iqsm::Quantum<Dependee>;

        static_assert(meta::operations::ExistenceNeed<Anchor, decltype(Need)>);

        const auto anchor_field = staged->field<Anchor>();
        const auto dependee_field = staged->field<Dependee>();

        for (const auto& kv : anchor_field->container) {
            const auto& id = kv.first;
            const auto& anchor_item = kv.second;
            const bool need = Need(staged, id, anchor_item);

            const bool has = dependee_field->container.contains(id);

            if (need) {
                if (has) continue;
                staged.add<Dependee>(id, base::make_shared<const DependeeQuantum>(DependeeQuantum{}));
            } else {
                if (not has) continue;
                const auto before = dependee_field->container.at(id);
                staged.remove<Dependee>(id, before);
            }
        }
    }

    template<meta::Aspect Dependee, meta::Aspect Anchor, auto Need, auto Construct>
    void logic::existence(Writing writing)
    {
        repo::Staged staged{writing};

        using AnchorId = Id<Anchor>;
        using AnchorItem = iqsm::Item<Anchor>;

        static_assert(meta::operations::ExistenceNeed<Anchor, decltype(Need)>);
        static_assert(meta::operations::DependeeConstruct<Anchor, decltype(Construct)>);

        const auto anchor_field = staged->field<Anchor>();
        const auto dependee_field = staged->field<Dependee>();

        for (const auto& kv : anchor_field->container) {
            const auto& id = kv.first;
            const auto& anchor_item = kv.second;
            const bool need = Need(staged, id, anchor_item);

            const bool has = dependee_field->container.contains(id);

            if (need) {
                if (has) continue;
                Construct(staged, id, anchor_item);
            } else {
                if (not has) continue;
                const auto before = dependee_field->container.at(id);
                staged.remove<Dependee>(id, before);
            }
        }
    }

    template<meta::Aspect Anchor, meta::Aspect Dependee, auto Construct>
    void structural::isomorphic(Writing writing)
    {
        repo::Staged staged{writing};

        using AnchorId = Id<Anchor>;
        using DependeeId = Id<Dependee>;
        using AnchorItem = iqsm::Item<Anchor>;
        static_assert(std::is_same_v<AnchorId, DependeeId>);
        static_assert(meta::operations::DependeeConstruct<Anchor, decltype(Construct)>);

        const auto anchor_field = staged->field<Anchor>();
        const auto dependee_field = staged->field<Dependee>();

        // Necessary: Dependee cannot outlive Anchor.
        for (const auto& kv : dependee_field->container) {
            const auto& dependee_id = kv.first;
            const auto& dependee_item = kv.second;
            if (not anchor_field->container.contains(dependee_id)) {
                staged.remove<Dependee>(dependee_id, dependee_item);
            }
        }

        // Sufficient: Anchor implies Dependee.
        for (const auto& kv : anchor_field->container) {
            const auto& anchor_id = kv.first;
            const auto& anchor_item = kv.second;
            if (not dependee_field->container.contains(anchor_id)) {
                Construct(staged, anchor_id, anchor_item);
            }
        }
    }

    template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
    // anchor(any):
    // - Dependee holds std::vector<Anchor::Id>
    // - prunes missing anchor ids from that vector
    // - deletes dependee only if the vector becomes empty
    void structural::anchor_any(Writing writing)
    {
        repo::Staged staged{writing};

        static_assert(std::is_member_object_pointer_v<decltype(Member)>);

        using Quantum = iqsm::Quantum<Dependee>;
        using AnchorId = Id<Anchor>;
        using MemberValue = std::remove_reference_t<decltype(std::declval<Quantum&>().*Member)>;
        static_assert(std::is_same_v<MemberValue, std::vector<AnchorId>>);

        const auto anchor_field = staged->field<Anchor>();
        const auto dependee_field = staged->field<Dependee>();

        for (const auto& kv : dependee_field->container) {
            const auto& dependee_id = kv.first;
            const auto& dependee_item = kv.second;

            const auto& ids = (dependee_item.get()->*Member);

            std::vector<AnchorId> filtered;
            filtered.reserve(ids.size());
            for (const auto& id : ids) {
                if (anchor_field->container.contains(id)) {
                    filtered.push_back(id);
                }
            }

            if (filtered.empty()) {
                staged.remove<Dependee>(dependee_id, dependee_item);
                continue;
            }

            if (filtered.size() != ids.size()) {
                Quantum q = *dependee_item;
                q.*Member = std::move(filtered);
                staged.update<Dependee>(dependee_id, dependee_item, base::make_shared<const Quantum>(std::move(q)));
            }
        }
    }

    template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
    // anchor(all):
    // - Dependee holds std::vector<Anchor::Id> or std::pair<Anchor::Id, Anchor::Id>
    // - deletes dependee if any anchor id is missing
    void structural::anchor_all(Writing writing)
    {
        repo::Staged staged{writing};

        static_assert(std::is_member_object_pointer_v<decltype(Member)>);

        using Quantum = iqsm::Quantum<Dependee>;
        using AnchorId = Id<Anchor>;
        using MemberValue = std::remove_reference_t<decltype(std::declval<Quantum&>().*Member)>;
        static_assert(
            std::is_same_v<MemberValue, std::vector<AnchorId>>
            || std::is_same_v<MemberValue, std::pair<AnchorId, AnchorId>>,
            "anchor_all: member must be vector<Anchor::Id> or pair<Anchor::Id, Anchor::Id>");

        const auto anchor_field = staged->field<Anchor>();
        const auto dependee_field = staged->field<Dependee>();

        if constexpr (std::is_same_v<MemberValue, std::vector<AnchorId>>) {
            for (const auto& kv : dependee_field->container) {
                const auto& dependee_id = kv.first;
                const auto& dependee_item = kv.second;

                const auto& ids = (dependee_item.get()->*Member);

                bool ok = true;
                for (const auto& id : ids) {
                    if (not anchor_field->container.contains(id)) {
                        ok = false;
                        break;
                    }
                }

                if (not ok) {
                    staged.remove<Dependee>(dependee_id, dependee_item);
                }
            }
        } else {
            for (const auto& kv : dependee_field->container) {
                const auto& dependee_id = kv.first;
                const auto& dependee_item = kv.second;

                const auto& pr = (dependee_item.get()->*Member);
                const bool ok = anchor_field->container.contains(pr.first)
                    && anchor_field->container.contains(pr.second);

                if (not ok) {
                    staged.remove<Dependee>(dependee_id, dependee_item);
                }
            }
        }
    }
} // namespace iqsm::operations::validation
