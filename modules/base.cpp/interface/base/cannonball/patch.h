#pragma once

#include <utility>

#include <base/cannonball/patchlet.h>
#include <base/cannonball/table.h>
#include <base/function_ref.h>

namespace base::cannonball {

// set of changes for some table
template<typename Key, typename Val>
class Patch : public Table<Key, Patchlet<Val>> {
public:
    using Base = Table<Key, Patchlet<Val>>;
    using RelatedOperational = table::Operational<Key, Val>;
    using RelatedDirect = table::Direct<Key, Val>;

    // Absent → insert; present → replace quantum+verified, tombstone |= incoming.
    Patchlet<Val>& insert(const Key& id, const Patchlet<Val>& patchlet);
    Patchlet<Val>& insert(Key&& id, Patchlet<Val>&& patchlet);

    // Honest put: write quantum, mark verified; never clears tombstone.
    void modify(const Key& id, const Val& quantum);
    void modify(Key&& id, Val&& quantum);
    // Touch / QuantumGate: ensure patchlet, return quantum ref; unverified when created; keeps tombstone.
    Val& modify_modification(Key, base::function_ref<const Val&()> prepatch);

    bool discard_changes(const Key& id);

    static void integrate(RelatedOperational& target, const Patch& patch);
    static void integrate(RelatedDirect& target, const Patch& patch);

    // (remove, update) = remove wins via tombstone. Other cases: right wins.
    static void merge(Patch& receiver, const Patch& other);
    static void merge_three_way(const RelatedOperational&, Patch& receiver, const Patch& other);
};

} // namespace base::cannonball

namespace base::cannonball {

template<typename Key, typename Val>
Patchlet<Val>& Patch<Key, Val>
::insert(const Key& id, const Patchlet<Val>& patchlet)
{
    if (auto* current = this->find(id)) {
        current->tombstone = current->tombstone || patchlet.tombstone;
        current->verified = patchlet.verified;
        current->quantum = patchlet.quantum;
        return *current;
    }

    Base::insert(id, patchlet);
    return *this->find(id);
}

template<typename Key, typename Val>
Patchlet<Val>& Patch<Key, Val>
::insert(Key&& id, Patchlet<Val>&& patchlet)
{
    if (auto* current = this->find(id)) {
        current->tombstone = current->tombstone || patchlet.tombstone;
        current->verified = patchlet.verified;
        current->quantum = std::move(patchlet.quantum);
        return *current;
    }

    const Key key = id;
    Base::insert(std::move(id), std::move(patchlet));
    return *this->find(key);
}

template<typename Key, typename Val>
void Patch<Key, Val>
::modify(const Key& id, const Val& quantum)
{
    insert(id, Patchlet<Val>::modification(quantum));
}

template<typename Key, typename Val>
void Patch<Key, Val>
::modify(Key&& id, Val&& quantum)
{
    insert(std::move(id), Patchlet<Val>::modification(std::move(quantum)));
}

template<typename Key, typename Val>
Val& Patch<Key, Val>
::modify_modification(Key id, base::function_ref<const Val&()> prepatch)
{
    if (auto* patchlet = Base::find(id)) {
        return patchlet->quantum;
    }

    const Key key = id;
    insert(std::move(id), Patchlet<Val>::possible(prepatch()));
    return Base::at(key).quantum;
}

template<typename Key, typename Val>
bool Patch<Key, Val>
::discard_changes(const Key& id)
{
    return Base::erase(id);
}

template<typename Key, typename Val>
void Patch<Key, Val>
::integrate(RelatedOperational& target, const Patch& patch)
{
    for (const auto entry : patch) {
        if (entry.value.tombstone) {
            target.erase(entry.id);
            continue;
        }

        target.insert(entry.id, entry.value.quantum);
    }
}

template<typename Key, typename Val>
void Patch<Key, Val>
::integrate(RelatedDirect& target, const Patch& patch)
{
    for (const auto entry : patch) {
        if (entry.value.tombstone) {
            target.erase(entry.id);
            continue;
        }

        if (auto* current = target.find(entry.id)) {
            *current = entry.value.quantum;
            continue;
        }

        target.insert(entry.id, entry.value.quantum);
    }
}

template<typename Key, typename Val>
void Patch<Key, Val>
::merge(Patch& receiver, const Patch& other)
{
    for (const auto entry : other)
        receiver.insert(entry.id, entry.value);
}

template<typename Key, typename Val>
void Patch<Key, Val>
::merge_three_way(const RelatedOperational&, Patch& receiver, const Patch& other)
{
    merge(receiver, other);
}

} // namespace base::cannonball
