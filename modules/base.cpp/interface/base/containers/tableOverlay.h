#pragma once

#include <memory>
#include <stdexcept>
#include <utility>

#include <base/containers/tableView.h>
#include <base/types/patches.h>

namespace base {

template<typename Key, typename Val>
class TableOverlay : public TableView<Key, Val> {
public:
    using View = TableView<Key, Val>;
    using Patch = types::Patch<Val>;
    using PatchView = TableView<Key, Patch>;

    TableOverlay(const View& state, const PatchView& patch)
        : state(state)
        , patch(patch)
    {}

    bool contains(const Key& key) const override {
        return find(key) != nullptr;
    }

    const Val* find(const Key& key) const override {
        if (const auto* patched = patch.find(key)) {
            if (!patched->has_value()) return nullptr;
            return std::addressof(patched->value());
        }

        return state.find(key);
    }

    const Val& at(const Key& key) const override {
        if (const auto* found = find(key)) return *found;
        throw std::out_of_range("TableOverlay::at");
    }

    std::size_t size() const override {
        std::size_t result = state.size();

        // TODO: consider cachind add/remove features inside of Patch to make this a bit faster:
        for (const auto entry : patch) {
            const bool existed = state.contains(entry.first);
            if (!entry.second.has_value()) {
                if (existed) --result;
                continue;
            }
            if (!existed) ++result;
        }

        return result;
    }

public:
    using ReadIterator = typename View::ReadIterator;
    using EntryView = typename View::EntryView;
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
            const TableOverlay& owner,
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
                if (const auto* patched = owner->patch.find(entry.first)) {
                    return EntryView{entry.first, patched->value()};
                }
                return EntryView{entry.first, entry.second};
            }

            const auto entry = *patchIt;
            return EntryView{entry.first, entry.second.value()};
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
                    const auto* patched = owner->patch.find(entry.first);

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

                    if (!entry.second.has_value() || owner->state.contains(entry.first)) {
                        ++patchIt;
                        continue;
                    }

                    return;
                }

                phase = Phase::end;
            }
        }

        const TableOverlay* owner;
        Phase phase;
        StateIterator stateIt;
        StateIterator stateEnd;
        PatchIterator patchIt;
        PatchIterator patchEnd;
    };

protected:
    ReadIterator read_begin() const override {
        return this->make_read_iterator(ConstIterator{
            *this,
            Phase::state,
            state.begin(),
            state.end(),
            patch.begin(),
            patch.end()
        });
    }

    ReadIterator read_end() const override {
        return this->make_read_iterator(ConstIterator{
            *this,
            Phase::end,
            state.begin(),
            state.end(),
            patch.begin(),
            patch.end()
        });
    }

private:
    const View& state;
    const PatchView& patch;
};

} // namespace base
