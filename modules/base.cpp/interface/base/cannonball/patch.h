#pragma once

#include <utility>

#include <base/cannonball/table.h>
#include <base/function_ref.h>

namespace base::cannonball {

// Change carrier for one id.
// tombstone: explicit deletion — survives quantum edits; wins on integrate.
// verified: Scarlett — true after honest put_*; false after reference/touch (modify_modification).
template<typename T>
struct Patchlet {
    bool tombstone = false;
    bool verified = false;
    T quantum;

    static Patchlet deletion(T quantum ) {
        return Patchlet{true, true, std::move(quantum)};
    }

    static Patchlet modification(T quantum) {
        return Patchlet{false, true, std::move(quantum)};
    }

    static Patchlet possible(T quantum) {
        return Patchlet{false, false, std::move(quantum)};
    }
};

// set of changes for some table
template<typename Key, typename Val, typename Hasher = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
class Patch : public Table<Key, Patchlet<Val>, Hasher, KeyEqual> {
public:
    using Base = Table<Key, Patchlet<Val>, Hasher, KeyEqual>;
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

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
Patchlet<Val>& Patch<Key, Val, Hasher, KeyEqual>
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

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
Patchlet<Val>& Patch<Key, Val, Hasher, KeyEqual>
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

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Patch<Key, Val, Hasher, KeyEqual>
::modify(const Key& id, const Val& quantum)
{
    insert(id, Patchlet<Val>::modification(quantum));
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Patch<Key, Val, Hasher, KeyEqual>
::modify(Key&& id, Val&& quantum)
{
    insert(std::move(id), Patchlet<Val>::modification(std::move(quantum)));
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
Val& Patch<Key, Val, Hasher, KeyEqual>
::modify_modification(Key id, base::function_ref<const Val&()> prepatch)
{
    if (auto* patchlet = Base::find(id)) {
        return patchlet->quantum;
    }

    const Key& key = id;
    insert(std::move(id), Patchlet<Val>::possible(prepatch()));
    return Base::at(key).quantum;
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
        if (entry.value.tombstone) {
            target.erase(entry.id);
            continue;
        }

        target.insert(entry.id, entry.value.quantum);
    }
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Patch<Key, Val, Hasher, KeyEqual>
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

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Patch<Key, Val, Hasher, KeyEqual>
::merge(Patch& receiver, const Patch& other)
{
    for (const auto entry : other)
        receiver.insert(entry.id, entry.value);
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Patch<Key, Val, Hasher, KeyEqual>
::merge_three_way(const RelatedOperational&, Patch& receiver, const Patch& other)
{
    merge(receiver, other);
}

} // namespace base::cannonball
