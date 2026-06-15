#pragma once

#include <optional>
#include <utility>

#include <base/cannonball/delta/interface.h>
#include <base/cannonball/table/read.h>
#include <base/cannonball/patch.h>

namespace base::cannonball::delta {

template<typename Key, typename Val>
class Operational : public Interface<Key, Val> {
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
            PatchIterator current,
            PatchIterator end,
            Layer layer)
            : state(std::addressof(state))
            , patch(std::addressof(patch))
            , patchCurrent(std::move(current))
            , patchEnd(std::move(end))
            , layer(layer)
        {
            skip_to_match();
        }

        auto operator*() const -> ChangeType {
            return make_patch_value(*patchCurrent);
        }

        IteratorImpl& operator++() {
            ++*patchCurrent;
            skip_to_match();
            return *this;
        }

        IteratorImpl operator++(int) {
            IteratorImpl copy = *this;
            ++*this;
            return copy;
        }

        bool operator==(const IteratorImpl& other) const {
            return state == other.state && patch == other.patch && patchCurrent == other.patchCurrent;
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
            while (patchCurrent != patchEnd) {
                if (matches(layer, make_patch_value(*patchCurrent))) return;
                ++*patchCurrent;
            }
        }

        auto make_patch_value(PatchIterator iterator) const -> ChangeType {
            const auto entry = *iterator;
            const auto* before = state->find(entry.key);
            const auto* after = entry.value.has_value() ? std::addressof(entry.value.value()) : nullptr;
            return ChangeType{entry.key, std::optional<const Val*>{before}, after};
        }

        const View* state;
        const PatchView* patch;
        std::optional<PatchIterator> patchCurrent;
        std::optional<PatchIterator> patchEnd;
        Layer layer;
    };

    Operational(const View& state, const PatchView& patch)
        : state(state)
        , patch(patch)
    {}

protected:
    auto delta_begin(Layer layer) const -> Iterator override {
        return this->make_delta_iterator(IteratorImpl{state, patch, patch.begin(), patch.end(), layer});
    }

    auto delta_end(Layer layer) const -> Iterator override {
        return this->make_delta_iterator(IteratorImpl{state, patch, patch.end(), patch.end(), layer});
    }

private:
    const View& state;
    const PatchView& patch;
};

} // namespace base::cannonball::delta