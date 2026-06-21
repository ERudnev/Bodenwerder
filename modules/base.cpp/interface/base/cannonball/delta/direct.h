#pragma once

#include <optional>
#include <utility>

#include <base/cannonball/delta/interface.h>
#include <base/cannonball/table/read.h>
#include <base/cannonball/patch.h>

namespace base::cannonball::delta {

template<typename Key, typename Val>
class Direct : public Interface<Key, Val> {
public:
    using InterfaceType = Interface<Key, Val>;
    using View = table::Read<Key, Val>;
    using PatchView = table::Read<Key, Patchlet<Val>>;
    using PatchIterator = typename PatchView::ReadIterator;
    using StateIterator = typename View::ReadIterator;
    using ChangeType = typename InterfaceType::value_type;
    using Layer = typename InterfaceType::Layer;
    using Iterator = typename InterfaceType::Iterator;

    class IteratorImpl {
    public:
        IteratorImpl(
            const View& state,
            const PatchView& patch,
            StateIterator current,
            StateIterator end,
            Layer layer,
            bool)
            : state(std::addressof(state))
            , patch(std::addressof(patch))
            , patchCurrent(patch.begin())
            , patchEnd(patch.end())
            , stateCurrent(std::move(current))
            , stateEnd(std::move(end))
            , layer(layer)
        {
            skip_to_match();
        }

        auto operator*() const -> ChangeType {
            if (stateCurrent != stateEnd) {
                return make_dirty_state_value(*stateCurrent);
            }
            return make_patch_only_value(*patchCurrent);
        }

        IteratorImpl& operator++() {
            if (stateCurrent != stateEnd) {
                ++*stateCurrent;
            } else {
                ++*patchCurrent;
            }
            skip_to_match();
            return *this;
        }

        IteratorImpl operator++(int) {
            IteratorImpl copy = *this;
            ++*this;
            return copy;
        }

        bool operator==(const IteratorImpl& other) const {
            return state == other.state
                && patch == other.patch
                && stateCurrent == other.stateCurrent
                && patchCurrent == other.patchCurrent;
        }

        bool operator!=(const IteratorImpl& other) const {
            return !(*this == other);
        }

    private:
        static bool matches(Layer layer, const ChangeType& change) {
            if (layer == Layer::all) return change.good();
            if (layer == Layer::added) return change.add();
            if (layer == Layer::addedOrUpdated) return change.addedOrUpdated();
            if (layer == Layer::removed) return change.remove();
            if (layer == Layer::updated) return change.update();
            return false;
        }

        void skip_to_match() {
            while (stateCurrent != stateEnd) {
                if (matches(layer, make_dirty_state_value(*stateCurrent))) return;
                ++*stateCurrent;
            }

            while (patchCurrent != patchEnd) {
                if (state->contains((*patchCurrent)->id)) {
                    ++*patchCurrent;
                    continue;
                }
                if (matches(layer, make_patch_only_value(*patchCurrent))) return;
                ++*patchCurrent;
            }
        }

        auto make_dirty_state_value(StateIterator iterator) const -> ChangeType {
            const auto entry = *iterator;
            if (const auto* patchEntry = patch->find(entry.id)) {
                const auto* after = patchEntry->has_value() ? std::addressof(patchEntry->value()) : nullptr;
                return ChangeType{entry.id, std::optional<const Val*>{std::addressof(entry.value)}, after};
            }
            return ChangeType{entry.id, std::nullopt, std::addressof(entry.value)};
        }

        auto make_patch_only_value(PatchIterator iterator) const -> ChangeType {
            const auto entry = *iterator;
            const auto* before = state->find(entry.id);
            const auto* after = entry.value.has_value() ? std::addressof(entry.value.value()) : nullptr;
            return ChangeType{entry.id, std::optional<const Val*>{before}, after};
        }

        const View* state;
        const PatchView* patch;
        std::optional<PatchIterator> patchCurrent;
        std::optional<PatchIterator> patchEnd;
        std::optional<StateIterator> stateCurrent;
        std::optional<StateIterator> stateEnd;
        Layer layer;
    };

    Direct(const View& state, const PatchView& patch)
        : state(state)
        , patch(patch)
    {}

protected:
    auto delta_begin(Layer layer) const -> Iterator override {
        return this->make_delta_iterator(IteratorImpl{state, patch, state.begin(), state.end(), layer, true});
    }

    auto delta_end(Layer layer) const -> Iterator override {
        return this->make_delta_iterator(IteratorImpl{state, patch, state.end(), state.end(), layer, true});
    }

private:
    const View& state;
    const PatchView& patch;
};

} // namespace base::cannonball::delta