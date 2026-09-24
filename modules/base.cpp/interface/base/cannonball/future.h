#pragma once

#include <memory>
#include <stdexcept>
#include <utility>

#include <base/cannonball/cursor.h>
#include <base/cannonball/table/operational.h>
#include <base/cannonball/patch.h>

namespace base::cannonball {

// TODO: fine better place for comment below.
// (a, set(a)) ~ set(a) *meaningless operation to get rid of equal(Val, Val) functions used here*
enum class SeeChanges {
    observable,
    blind,
};

template<typename Key, typename Val>
class Future : public table::Operational<Key, Val> {
public:
    using Mode = SeeChanges;
    using Interface = table::Operational<Key, Val>;
    using View = table::Read<Key, Val>;
    using PatchType = Patch<Key, Val>;
    using PatchView = table::Read<Key, Patchlet<Val>>;

    using SizeType = typename Interface::SizeType;
    using EntryView = typename View::EntryView;
    using Cursor = typename View::Cursor;

    Future(const View& state, PatchType& patch, SeeChanges mode);

    bool contains(const Key& id) const override;
    const Val* find(const Key& id) const override;
    const Val& at(const Key& id) const override;
    SizeType size() const override;

    void clear() override;
    void reserve(SizeType capacity) override;
    Val& insert(const Key& id, const Val& value) override;
    Val& insert(Key&& id, Val&& value) override;
    bool erase(const Key& id) override;

protected:
    Cursor read_begin() const override;
    Cursor read_end() const override;


//private:
// this breach is made for outer wrapping fqsm::model::linear::Future to get acces for "WorkerInterface"
// TODO: make this better and restore encapsulation:
public:
    auto patch_view() const -> const PatchView&;

    const View& state;
    PatchType& patch;
    const SeeChanges mode;
};

} // namespace base::cannonball

// Impl
namespace base::cannonball {

template<typename Key, typename Val>
Future<Key, Val>::Future(const View& state, PatchType& patch, SeeChanges mode)
    : state(state)
    , patch(patch)
    , mode(mode)
{}

template<typename Key, typename Val>
bool Future<Key, Val>::contains(const Key& id) const
{
    return find(id) != nullptr;
}

template<typename Key, typename Val>
const Val* Future<Key, Val>::find(const Key& id) const
{
    if (mode == SeeChanges::blind)
        return state.find(id);

    if (const auto* patched = patch_view().find(id)) {
        if (patched->tombstone) return nullptr;
        return std::addressof(patched->quantum);
    }

    return state.find(id);
}

template<typename Key, typename Val>
const Val& Future<Key, Val>::at(const Key& id) const
{
    if (const auto* found = find(id)) return *found;
    throw std::out_of_range("Future::at");
}

template<typename Key, typename Val>
auto Future<Key, Val>::size() const -> SizeType
{
    if (mode == SeeChanges::blind)
        return state.size();

    SizeType result = state.size();

    for (const auto entry : patch_view()) {
        const bool existed = state.contains(entry.id);
        if (entry.value.tombstone) {
            if (existed) --result;
            continue;
        }

        if (!existed) ++result;
    }

    return result;
}

template<typename Key, typename Val>
void Future<Key, Val>::clear()
{
    patch.clear();
    patch.reserve(state.size());

    for (const auto entry : state)
        patch.insert(entry.id, Patchlet<Val>::deletion(entry.value));
}

template<typename Key, typename Val>
void Future<Key, Val>::reserve(SizeType capacity)
{
    patch.reserve(capacity);
}

template<typename Key, typename Val>
Val& Future<Key, Val>::insert(const Key& id, const Val& value)
{
    return patch.insert(id, Patchlet<Val>::modification(value)).quantum;
}

template<typename Key, typename Val>
Val& Future<Key, Val>::insert(Key&& id, Val&& value)
{
    return patch.insert(std::move(id), Patchlet<Val>::modification(std::move(value))).quantum;
}

template<typename Key, typename Val>
bool Future<Key, Val>::erase(const Key& id)
{
    const bool existed_in_state = state.contains(id);
    const auto* patched = patch.find(id);

    if (patched) {
        if (patched->tombstone) return false;

        if (!existed_in_state) return patch.discard_changes(id);

        patch.insert(id, Patchlet<Val>::deletion(patched->quantum));
        return true;
    }

    if (!existed_in_state) return false;

    patch.insert(id, Patchlet<Val>::deletion(*state.find(id)));
    return true;
}

template<typename Key, typename Val>
auto Future<Key, Val>::read_begin() const -> Cursor
{
    if (mode == SeeChanges::blind)
        return state.begin();

    return Cursor::overlay(state.begin(), patch.raw_entries(), std::addressof(patch_view()), false);
}

template<typename Key, typename Val>
auto Future<Key, Val>::read_end() const -> Cursor
{
    if (mode == SeeChanges::blind)
        return state.end();

    return Cursor::overlay(state.end(), patch.raw_entries(), std::addressof(patch_view()), true);
}

template<typename Key, typename Val>
auto Future<Key, Val>::patch_view() const -> const PatchView&
{
    return static_cast<const PatchView&>(patch);
}

} // namespace base::cannonball
