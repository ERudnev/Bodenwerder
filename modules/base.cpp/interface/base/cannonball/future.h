#pragma once

#include <memory>
#include <stdexcept>
#include <utility>

#include <base/cannonball/table/operational.h>
#include <base/cannonball/patch.h>

namespace base::cannonball {

// TODO: fine better place for comment below.
// (a, set(a)) ~ set(a) *meaningless operation to get rid of equal(Val, Val) functions used here*
enum class SeeChanges {
    observable,
    blind,
};

template<typename Key, typename Val, typename Hasher = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
class Future : public table::Operational<Key, Val> {
public:
    using Mode = SeeChanges;
    using Interface = table::Operational<Key, Val>;
    using View = table::Read<Key, Val>;
    using PatchType = Patch<Key, Val, Hasher, KeyEqual>;
    using PatchView = table::Read<Key, Patchlet<Val>>;

    using SizeType = typename Interface::SizeType;
    using EntryView = typename View::EntryView;
    using ReadIterator = typename View::ReadIterator;
    using StateIterator = typename View::ReadIterator;
    using PatchIterator = typename PatchView::ReadIterator;

    enum class Phase {
        state,
        patch,
        end
    };

    class ConstIterator {
    public:
        ConstIterator(
            const Future& owner,
            Phase phase,
            StateIterator stateIt,
            StateIterator stateEnd,
            PatchIterator patchIt,
            PatchIterator patchEnd)
            : owner(std::addressof(owner))
            , phase(phase)
            , stateIt(std::move(stateIt))
            , stateEnd(std::move(stateEnd))
            , patchIt(std::move(patchIt))
            , patchEnd(std::move(patchEnd))
        {
            skip_to_visible();
        }

        EntryView operator*() const {
            if (phase == Phase::state) {
                const auto entry = *stateIt;
                if (const auto* patched = owner->patch_view().find(entry.id))
                    return EntryView{entry.id, patched->value()};
                return EntryView{entry.id, entry.value};
            }

            const auto entry = *patchIt;
            return EntryView{entry.id, entry.value.value()};
        }

        ConstIterator& operator++() {
            if (phase == Phase::state) {
                ++stateIt;
            } else if (phase == Phase::patch) {
                ++patchIt;
            }

            skip_to_visible();
            return *this;
        }

        ConstIterator operator++(int) {
            ConstIterator copy = *this;
            ++*this;
            return copy;
        }

        bool operator==(const ConstIterator& other) const {
            if (owner != other.owner || phase != other.phase) return false;
            if (phase == Phase::end) return true;
            if (phase == Phase::state) return stateIt == other.stateIt;
            return patchIt == other.patchIt;
        }

        bool operator!=(const ConstIterator& other) const {
            return !(*this == other);
        }

    private:
        void skip_to_visible() {
            if (phase == Phase::end) return;

            while (phase == Phase::state) {
                while (stateIt != stateEnd) {
                    const auto entry = *stateIt;
                    const auto* patched = owner->patch_view().find(entry.id);

                    if (patched && !patched->has_value()) {
                        ++stateIt;
                        continue;
                    }

                    return;
                }

                phase = Phase::patch;
            }

            while (phase == Phase::patch) {
                while (patchIt != patchEnd) {
                    const auto entry = *patchIt;

                    if (!entry.value.has_value() || owner->state.contains(entry.id)) {
                        ++patchIt;
                        continue;
                    }

                    return;
                }

                phase = Phase::end;
            }
        }

        const Future* owner;
        Phase phase;
        StateIterator stateIt;
        StateIterator stateEnd;
        PatchIterator patchIt;
        PatchIterator patchEnd;
    };

    Future(const View& state, PatchType& patch, SeeChanges mode);

    bool contains(const Key& id) const override;
    const Val* find(const Key& id) const override;
    const Val& at(const Key& id) const override;
    SizeType size() const override;

    void clear() override;
    void reserve(SizeType capacity) override;
    void insert(const Key& id, const Val& value) override;
    void insert(Key&& id, Val&& value) override;
    bool erase(const Key& id) override;

protected:
    ReadIterator read_begin() const override;
    ReadIterator read_end() const override;

private:
    auto patch_view() const -> const PatchView&;

    const View& state;
    PatchType& patch;
    const SeeChanges mode;
};

} // namespace base::cannonball

// Impl
namespace base::cannonball {

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
Future<Key, Val, Hasher, KeyEqual>::Future(const View& state, PatchType& patch, SeeChanges mode)
    : state(state)
    , patch(patch)
    , mode(mode)
{}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
bool Future<Key, Val, Hasher, KeyEqual>::contains(const Key& id) const
{
    return find(id) != nullptr;
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
const Val* Future<Key, Val, Hasher, KeyEqual>::find(const Key& id) const
{
    if (mode == SeeChanges::blind)
        return state.find(id);

    if (const auto* patched = patch_view().find(id)) {
        if (!patched->has_value()) return nullptr;
        return std::addressof(patched->value());
    }

    return state.find(id);
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
const Val& Future<Key, Val, Hasher, KeyEqual>::at(const Key& id) const
{
    if (const auto* found = find(id)) return *found;
    throw std::out_of_range("Future::at");
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
auto Future<Key, Val, Hasher, KeyEqual>::size() const -> SizeType
{
    if (mode == SeeChanges::blind)
        return state.size();

    SizeType result = state.size();

    for (const auto entry : patch_view()) {
        const bool existed = state.contains(entry.id);
        if (!entry.value.has_value()) {
            if (existed) --result;
            continue;
        }

        if (!existed) ++result;
    }

    return result;
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Future<Key, Val, Hasher, KeyEqual>::clear()
{
    patch.clear();
    patch.reserve(state.size());

    for (const auto entry : state)
        patch.insert(entry.id, std::nullopt);
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Future<Key, Val, Hasher, KeyEqual>::reserve(SizeType capacity)
{
    patch.reserve(capacity);
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Future<Key, Val, Hasher, KeyEqual>::insert(const Key& id, const Val& value)
{
    patch.insert(id, Patchlet<Val>{value});
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
void Future<Key, Val, Hasher, KeyEqual>::insert(Key&& id, Val&& value)
{
    patch.insert(std::move(id), Patchlet<Val>{std::move(value)});
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
bool Future<Key, Val, Hasher, KeyEqual>::erase(const Key& id)
{
    const bool existed_in_state = state.contains(id);
    const auto* patched = patch.find(id);

    if (patched) {
        if (!patched->has_value()) return false;

        if (!existed_in_state) return patch.discard_changes(id);

        patch.insert(id, std::nullopt);
        return true;
    }

    if (!existed_in_state) return false;

    patch.insert(id, std::nullopt);
    return true;
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
auto Future<Key, Val, Hasher, KeyEqual>::read_begin() const -> ReadIterator
{
    if (mode == SeeChanges::blind)
        return state.begin();

    return this->make_read_iterator(ConstIterator{
        *this,
        Phase::state,
        state.begin(),
        state.end(),
        patch_view().begin(),
        patch_view().end()
    });
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
auto Future<Key, Val, Hasher, KeyEqual>::read_end() const -> ReadIterator
{
    if (mode == SeeChanges::blind)
        return state.end();

    return this->make_read_iterator(ConstIterator{
        *this,
        Phase::end,
        state.begin(),
        state.end(),
        patch_view().begin(),
        patch_view().end()
    });
}

template<typename Key, typename Val, typename Hasher, typename KeyEqual>
auto Future<Key, Val, Hasher, KeyEqual>::patch_view() const -> const PatchView&
{
    return static_cast<const PatchView&>(patch);
}

} // namespace base::cannonball