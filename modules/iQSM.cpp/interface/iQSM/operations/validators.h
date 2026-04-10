#pragma once

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>
#include <memory>

#include <iQSM/world.h>
#include <iQSM/delta.h>
#include <iQSM/internals/lifecycle_actions.h>
#include <iQSM/operations/integration.h>
#include <iQSM/types.h>
#include <iQSM/repository/commit.h>
#include <iQSM/repository/staged.h>



namespace iqsm::operations::validation {

    namespace helpers {
        // Applies Func to each (id, item) in Field<Meta> and stages updates:
        // - Func(World, Id<Meta>, const Quantum<Meta>&) -> std::optional<Quantum<Meta>>
        // - if Func returns {}, no change
        // - otherwise updates item
        template<meta::Aspect Meta, auto Func>
        void for_each_item(repo::Commit commit);
    }

    namespace structural {
        template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
        void anchor(repo::Commit);

        template<meta::Aspect Anchor, meta::Aspect Dependee>
        void anchor_attribute(repo::Commit); // confinement

        template<meta::Aspect Anchor, meta::Aspect Dependee, auto Construct>
        void isomorphic(repo::Commit); // 1-1 iff confinement

        // Like anchor(), but the member is optional-like:
        // - if optional has no value -> ignored
        // - if optional has value and Anchor is missing -> Dependee is removed
        template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
        void anchor_optional(repo::Commit);

        template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
        void anchor_any(repo::Commit);

        template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
        void anchor_all(repo::Commit);
    }

    namespace logic {
        // Ensures Dependee existence from Anchor by predicate.
        template<meta::Aspect Dependee, meta::Aspect Anchor, auto Need>
        void existence(repo::Commit);

        // Like existence(), but uses Construct to build new Dependee items when needed.
        // Construct signature: DependeeQuantum(World, AnchorId, Item<Anchor>).
        template<meta::Aspect Dependee, meta::Aspect Anchor, auto Need, auto Construct>
        void existence(repo::Commit);
    }
} // namespace iqsm::operations::validation

namespace iqsm::detail::operations::validation {
    template<meta::Aspect Anchor, meta::Aspect Dependee, typename Extract>
    void anchor_impl(repo::Commit commit, Extract extract)
    {
        const auto world = commit.initial;
        internals::FieldsMutable acc{};

        const auto anchor_field = world->field<Anchor>();
        const auto dependee_field = world->field<Dependee>();
        using Operation = typename delta::FieldDiff<Dependee>::Operation;

        for (const auto& kv : dependee_field->container) {
            const auto& dependee_id = kv.first;
            const auto& dependee_item = kv.second;

            const auto anchor_id = extract(dependee_id, dependee_item);
            if (not anchor_field->container.contains(anchor_id)) {
                detail::lifecycle::pre_remove_action_into_accumulator<Dependee>(world, acc, dependee_id, dependee_item);
                acc.add_op<Dependee>(dependee_id, Operation{ dependee_item, std::nullopt });
            }
        }

        if (not acc.empty()) {
            commit.push(acc.push());
        }
    }
} // namespace iqsm::detail::operations::validation

namespace iqsm::operations::validation {
    template<meta::Aspect Meta, auto Func>
    void helpers::for_each_item(repo::Commit commit) {
        using Id = ::iqsm::Id<Meta>;
        using Quantum = ::iqsm::Quantum<Meta>;
        using Item = ::iqsm::Item<Meta>;

        static_assert(std::is_invocable_r_v<std::optional<Quantum>, decltype(Func), World, Id, const Quantum&>);

        repo::Staged staged{commit};
        const auto world = commit.initial;
        const auto field = world->field<Meta>();

        for (const auto& kv : field->container) {
            const auto& id = kv.first;
            const Item& before = kv.second;

            const auto after_q = std::invoke(Func, world, id, *before);
            if (not after_q.has_value()) continue;
            staged.update<Meta>(id, before, base::make_shared<const Quantum>(std::move(*after_q)));
        }
    }

    template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
    void structural::anchor(repo::Commit commit)
    {
        static_assert(std::is_member_object_pointer_v<decltype(Member)>);

        using Quantum = iqsm::Quantum<Dependee>;
        using AnchorId = Id<Anchor>;
        using MemberValue = decltype(std::declval<Quantum>().*Member);
        static_assert(std::is_convertible_v<MemberValue, AnchorId>);

        return detail::operations::validation::anchor_impl<Anchor, Dependee>(std::move(commit), [](auto, auto dependee_item) -> AnchorId {
            return static_cast<AnchorId>(dependee_item.get()->*Member);
        });
    }

    template<meta::Aspect Anchor, meta::Aspect Dependee>
    void structural::anchor_attribute(repo::Commit commit)
    {
        return detail::operations::validation::anchor_impl<Anchor, Dependee>(commit, [](auto dependee_id, auto) { return dependee_id; });
    }

    template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
    void structural::anchor_optional(repo::Commit commit)
    {
        static_assert(std::is_member_object_pointer_v<decltype(Member)>);

        const auto world = commit.initial;
        internals::FieldsMutable acc{};

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

        const auto anchor_field = world->field<Anchor>();
        const auto dependee_field = world->field<Dependee>();

        using Operation = typename delta::FieldDiff<Dependee>::Operation;

        for (const auto& kv : dependee_field->container) {
            const auto& dependee_id = kv.first;
            const auto& dependee_item = kv.second;

            const auto& opt = (dependee_item.get()->*Member);
            if (not opt.has_value()) continue;

            const auto anchor_id = static_cast<AnchorId>(*opt);
            if (not anchor_field->container.contains(anchor_id)) {
                detail::lifecycle::pre_remove_action_into_accumulator<Dependee>(world, acc, dependee_id, dependee_item);
                acc.add_op<Dependee>(dependee_id, Operation{ dependee_item, std::nullopt });
            }
        }

        if (not acc.empty()) {
            commit.push(acc.push());
        }
    }

    template<meta::Aspect Dependee, meta::Aspect Anchor, auto Need>
    void logic::existence(repo::Commit commit)
    {
        const auto world = commit.initial;
        internals::FieldsMutable acc{};

        using AnchorId = Id<Anchor>;
        using AnchorItem = iqsm::Item<Anchor>;
        using DependeeQuantum = iqsm::Quantum<Dependee>;

        static_assert(std::is_invocable_r_v<bool, decltype(Need), World, AnchorId, AnchorItem>);

        const auto anchor_field = world->field<Anchor>();
        const auto dependee_field = world->field<Dependee>();

        using Operation = typename delta::FieldDiff<Dependee>::Operation;

        for (const auto& kv : anchor_field->container) {
            const auto& id = kv.first;
            const auto& anchor_item = kv.second;
            const bool need = Need(world, id, anchor_item);

            const bool has = dependee_field->container.contains(id);

            if (need) {
                if (has) continue;
                acc.add_op<Dependee>(id, Operation{ std::nullopt, base::make_shared<const DependeeQuantum>(DependeeQuantum{}) });
            } else {
                if (not has) continue;
                const auto before = dependee_field->container.at(id);
                detail::lifecycle::pre_remove_action_into_accumulator<Dependee>(world, acc, id, before);
                acc.add_op<Dependee>(id, Operation{ before, std::nullopt });
            }
        }

        if (not acc.empty()) {
            commit.push(acc.push());
        }
    }

    template<meta::Aspect Dependee, meta::Aspect Anchor, auto Need, auto Construct>
    void logic::existence(repo::Commit commit)
    {
        const auto world = commit.initial;
        internals::FieldsMutable acc{};

        using AnchorId = Id<Anchor>;
        using AnchorItem = iqsm::Item<Anchor>;
        using DependeeQuantum = iqsm::Quantum<Dependee>;

        static_assert(std::is_invocable_r_v<bool, decltype(Need), World, AnchorId, AnchorItem>);
        static_assert(std::is_invocable_r_v<DependeeQuantum, decltype(Construct), World, AnchorId, AnchorItem>);

        const auto anchor_field = world->field<Anchor>();
        const auto dependee_field = world->field<Dependee>();

        using Operation = typename delta::FieldDiff<Dependee>::Operation;

        for (const auto& kv : anchor_field->container) {
            const auto& id = kv.first;
            const auto& anchor_item = kv.second;
            const bool need = Need(world, id, anchor_item);

            const bool has = dependee_field->container.contains(id);

            if (need) {
                if (has) continue;
                acc.add_op<Dependee>(id, Operation{ std::nullopt, base::make_shared<const DependeeQuantum>(Construct(world, id, anchor_item)) });
            } else {
                if (not has) continue;
                const auto before = dependee_field->container.at(id);
                detail::lifecycle::pre_remove_action_into_accumulator<Dependee>(world, acc, id, before);
                acc.add_op<Dependee>(id, Operation{ before, std::nullopt });
            }
        }

        if (not acc.empty()) {
            commit.push(acc.push());
        }
    }

    template<meta::Aspect Anchor, meta::Aspect Dependee, auto Construct>
    void structural::isomorphic(repo::Commit commit)
    {
        const auto world = commit.initial;
        internals::FieldsMutable acc{};

        using AnchorId = Id<Anchor>;
        using DependeeId = Id<Dependee>;
        using AnchorQuantum = iqsm::Quantum<Anchor>;
        using AnchorItem = iqsm::Item<Anchor>;
        using DependeeQuantum = iqsm::Quantum<Dependee>;
        static_assert(std::is_same_v<AnchorId, DependeeId>);
        static_assert(std::is_invocable_r_v<DependeeQuantum, decltype(Construct), World, AnchorId, AnchorItem>);

        const auto anchor_field = world->field<Anchor>();
        const auto dependee_field = world->field<Dependee>();

        using Operation = typename delta::FieldDiff<Dependee>::Operation;

        // Necessary: Dependee cannot outlive Anchor.
        for (const auto& kv : dependee_field->container) {
            const auto& dependee_id = kv.first;
            const auto& dependee_item = kv.second;
            if (not anchor_field->container.contains(dependee_id)) {
                detail::lifecycle::pre_remove_action_into_accumulator<Dependee>(world, acc, dependee_id, dependee_item);
                acc.add_op<Dependee>(dependee_id, Operation{ dependee_item, std::nullopt });
            }
        }

        // Sufficient: Anchor implies Dependee.
        for (const auto& kv : anchor_field->container) {
            const auto& anchor_id = kv.first;
            const auto& anchor_item = kv.second;
            if (not dependee_field->container.contains(anchor_id)) {
                acc.add_op<Dependee>(anchor_id, Operation{ std::nullopt, base::make_shared<const DependeeQuantum>(Construct(world, anchor_id, anchor_item)) });
            }
        }

        if (not acc.empty()) {
            commit.push(acc.push());
        }
    }

    template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
    // anchor(any):
    // - Dependee holds std::vector<Anchor::Id>
    // - prunes missing anchor ids from that vector
    // - deletes dependee only if the vector becomes empty
    void structural::anchor_any(repo::Commit commit)
    {
        const auto world = commit.initial;
        internals::FieldsMutable acc{};

        static_assert(std::is_member_object_pointer_v<decltype(Member)>);

        using Quantum = iqsm::Quantum<Dependee>;
        using AnchorId = Id<Anchor>;
        using MemberValue = std::remove_reference_t<decltype(std::declval<Quantum&>().*Member)>;
        static_assert(std::is_same_v<MemberValue, std::vector<AnchorId>>);

        const auto anchor_field = world->field<Anchor>();
        const auto dependee_field = world->field<Dependee>();

        using Operation = typename delta::FieldDiff<Dependee>::Operation;

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
                detail::lifecycle::pre_remove_action_into_accumulator<Dependee>(world, acc, dependee_id, dependee_item);
                acc.add_op<Dependee>(dependee_id, Operation{ dependee_item, std::nullopt });
                continue;
            }

            if (filtered.size() != ids.size()) {
                Quantum q = *dependee_item;
                q.*Member = std::move(filtered);
                acc.add_op<Dependee>(dependee_id, Operation{ dependee_item, base::make_shared<const Quantum>(std::move(q)) });
            }
        }

        if (not acc.empty()) {
            commit.push(acc.push());
        }
    }

    template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
    // anchor(all):
    // - Dependee holds std::vector<Anchor::Id>
    // - deletes dependee if any anchor id is missing
    void structural::anchor_all(repo::Commit commit)
    {
        const auto world = commit.initial;
        internals::FieldsMutable acc{};

        static_assert(std::is_member_object_pointer_v<decltype(Member)>);

        using Quantum = iqsm::Quantum<Dependee>;
        using AnchorId = Id<Anchor>;
        using MemberValue = std::remove_reference_t<decltype(std::declval<Quantum&>().*Member)>;
        static_assert(std::is_same_v<MemberValue, std::vector<AnchorId>>);

        const auto anchor_field = world->field<Anchor>();
        const auto dependee_field = world->field<Dependee>();

        using Operation = typename delta::FieldDiff<Dependee>::Operation;

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
                detail::lifecycle::pre_remove_action_into_accumulator<Dependee>(world, acc, dependee_id, dependee_item);
                acc.add_op<Dependee>(dependee_id, Operation{ dependee_item, std::nullopt });
            }
        }

        if (not acc.empty()) {
            commit.push(acc.push());
        }
    }
} // namespace iqsm::operations::validation
