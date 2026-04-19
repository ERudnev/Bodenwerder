#pragma once

#include <optional>
#include <type_traits>

#include <iQSM/_forwards.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/meta/facade.h>
#include <iQSM/repository/transactions/staged.h>

namespace iqsm::meta::operations {

/// Predicate for `logic::existence`: whether a dependee row must exist for this anchor id (reading context).
template<typename Anchor, typename F>
concept ExistenceNeed =
    meta::Aspect<Anchor> && std::is_invocable_r_v<bool, F, World, Id<Anchor>, Item<Anchor>>;

/// Materialize dependee by writing through the validation transaction; call site is `Construct(staged, id, item)`.
/// First parameter is checked as `repo::Staged&` (not `Writing` by value: `Permit` is a poor fit for `std::declval` / trait checks).
template<typename Anchor, typename F>
concept DependeeConstruct =
    meta::Aspect<Anchor> && std::is_invocable_r_v<void, F, repo::Staged&, Id<Anchor>, Item<Anchor>>;

/// Per-item logical update for `helpers::for_each_item`: optional replacement quantum.
template<typename Meta, typename F>
concept ForEachItemUpdate =
    meta::Aspect<Meta> &&
    std::is_invocable_r_v<std::optional<Quantum<Meta>>, F, World, Id<Meta>, const Quantum<Meta>&>;

} // namespace iqsm::meta::operations
