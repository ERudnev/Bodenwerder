#pragma once

#include <functional>
#include <optional>
#include <utility>

#include <base/cannonball/table.h>

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
    void modify(const Key& key, const Val& value);
    void modify(Key&& key, Val&& value);

    // Forget local change for key completely.
    bool discard_changes(const Key& key);

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
void Patch<Key, Val, Hasher, KeyEqual>::modify(const Key& key, const Val& value)
{
    const auto* current = this->find(key);
    if (current && !current->has_value()) return;

    Base::insert(key, Patchlet<Val>{value});
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Patch<Key, Val, Hasher, KeyEqual>::modify(Key&& key, Val&& value)
{
    const auto* current = this->find(key);
    if (current && !current->has_value()) return;

    Base::insert(std::move(key), Patchlet<Val>{std::move(value)});
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
bool Patch<Key, Val, Hasher, KeyEqual>::discard_changes(const Key& key)
{
    return Base::erase(key);
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Patch<Key, Val, Hasher, KeyEqual>::integrate(RelatedOperational& target, const Patch& patch)
{
    for (const auto entry : patch) {
        if (!entry.value.has_value()) {
            target.erase(entry.key);
            continue;
        }

        target.insert(entry.key, entry.value.value());
    }
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Patch<Key, Val, Hasher, KeyEqual>::integrate(RelatedDirect& target, const Patch& patch)
{
    for (const auto entry : patch) {
        if (!entry.value.has_value()) {
            target.erase(entry.key);
            continue;
        }

        if (auto* current = target.find(entry.key)) {
            *current = entry.value.value();
            continue;
        }

        target.insert(entry.key, entry.value.value());
    }
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Patch<Key, Val, Hasher, KeyEqual>::merge(Patch& receiver, const Patch& other)
{
    for (const auto entry : other) {
        if (!entry.value.has_value()) {
            receiver.Base::insert(entry.key, std::nullopt);
            continue;
        }

        receiver.modify(entry.key, entry.value.value());
    }
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Patch<Key, Val, Hasher, KeyEqual>::merge_three_way(const RelatedOperational&, Patch& receiver, const Patch& other)
{
    // placeholder
    merge(receiver, other);
}

} // namespace base::cannonball