#pragma once

#include <functional>
#include <optional>
#include <utility>

#include <base/cannonball/table.h>
#include <base/function_ref.h>

namespace base::cannonball {

// elementary (minimal possible) data update carrier.
template<typename T>
using Patchlet = std::optional<T>;

// set of changes for some table
template<typename Key, typename Val, typename Hasher = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
class Patch : public Table<Key, Patchlet<Val>, Hasher, KeyEqual> {
public:
    using Base = Table<Key, Patchlet<Val>, Hasher, KeyEqual>;
    using RelatedOperational = table::Operational<Key, Val>;
    using RelatedDirect = table::Direct<Key, Val>;

    // Unlike Table::insert, replacing deletion keeps deletion.
    void modify(const Key& id, const Val& value); // DOTO: consider Key by its value here
    void modify(Key&& id, Val&& value);
    Val& modify_modification(Key, base::function_ref<const Val&()> prepatch);

    // Forget local change for id completely.
    bool discard_changes(const Key& id);

    // Mutates target with current patchlets. No normalization is implied.
    static void integrate(RelatedOperational& target, const Patch& patch);
    // Same semantics, but may use Val& access if target is Direct.
    static void integrate(RelatedDirect& target, const Patch& patch);

    // (remove, update) = remove. Other cases: right wins.
    static void merge(Patch& receiver, const Patch& other);
    // Placeholder-level implementation for now: equal to merge(receiver, other),
    // reference is intentionally ignored.
    static void merge_three_way(const RelatedOperational&, Patch& receiver, const Patch& other);
};

} // namespace base::cannonball

namespace base::cannonball {

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Patch<Key, Val, Hasher, KeyEqual>
::modify(const Key& id, const Val& value)
{
    const auto* current = this->find(id);
    if (current && !current->has_value()) return;

    Base::insert(id, Patchlet<Val>{value});
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Patch<Key, Val, Hasher, KeyEqual>
::modify(Key&& id, Val&& value)
{
    const auto* current = this->find(id);
    if (current && !current->has_value()) return;

    Base::insert(std::move(id), Patchlet<Val>{std::move(value)});
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
Val& Patch<Key, Val, Hasher, KeyEqual>
::modify_modification(Key id, base::function_ref<const Val&()> prepatch)
{
    if (auto* patchlet = Base::find(id))
        return patchlet->value();

    const Key& key = id;
    Base::insert(std::move(id), Patchlet<Val>{prepatch()});
    return Base::at(key).value();
}


template<typename Key, typename Val, typename Hasher, typename KeyEqual>
bool Patch<Key, Val, Hasher, KeyEqual>
::discard_changes(const Key& id)
{
    return Base::erase(id);
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Patch<Key, Val, Hasher, KeyEqual>
::integrate(RelatedOperational& target, const Patch& patch)
{
    for (const auto entry : patch) {
        if (!entry.value.has_value()) {
            target.erase(entry.id);
            continue;
        }

        target.insert(entry.id, entry.value.value());
    }
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Patch<Key, Val, Hasher, KeyEqual>
::integrate(RelatedDirect& target, const Patch& patch)
{
    for (const auto entry : patch) {
        if (!entry.value.has_value()) {
            target.erase(entry.id);
            continue;
        }

        if (auto* current = target.find(entry.id)) {
            *current = entry.value.value();
            continue;
        }

        target.insert(entry.id, entry.value.value());
    }
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Patch<Key, Val, Hasher, KeyEqual>
::merge(Patch& receiver, const Patch& other)
{
    for (const auto entry : other) {
        if (!entry.value.has_value()) {
            receiver.Base::insert(entry.id, std::nullopt);
            continue;
        }

        receiver.modify(entry.id, entry.value.value());
    }
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Patch<Key, Val, Hasher, KeyEqual>
::merge_three_way(const RelatedOperational&, Patch& receiver, const Patch& other)
{
    // placeholder
    merge(receiver, other);
}

} // namespace base::cannonball