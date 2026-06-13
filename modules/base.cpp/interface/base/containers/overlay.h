#pragma once

#include <memory>
#include <stdexcept>
#include <utility>

#include <base/containers/delta.h>
#include <base/containers/interface/write.h>
#include <base/containers/patch.h>

namespace base {

template<typename Key, typename Val>
class Overlay : public table::Write<Key, Val> {
public:
    using Interface = table::Write<Key, Val>;
    using View = table::Read<Key, Val>;
    using PatchElement = patch::Element<Val>;
    using Patch = table::Write<Key, PatchElement>;

    using KeyType = typename Interface::KeyType;
    using MappedType = typename Interface::MappedType;
    using SizeType = typename Interface::SizeType;

    using EntryView = typename Interface::EntryView;
    using ReadIterator = typename Interface::ReadIterator;
    using StateIterator = typename View::ReadIterator;
    using PatchIterator = typename Patch::ReadIterator;
    using ChangesView = typename Patch::EntryView;
    using ChangesIterator = typename Patch::ReadIterator;
    using DeltaView = Delta<Key, Val>;

    enum class Phase {
        state,
        patch,
        end
    };

    class ConstIterator {
    public:
        ConstIterator(
            const Overlay& owner,
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

        const Overlay* owner;
        Phase phase;
        StateIterator stateIt;
        StateIterator stateEnd;
        PatchIterator patchIt;
        PatchIterator patchEnd;
    };

    struct Changes {
        const Overlay* owner;

        auto begin() const -> ChangesIterator {
            return owner->changes_begin();
        }

        auto end() const -> ChangesIterator {
            return owner->changes_end();
        }
    };

    Overlay(const View& state, Patch& patch)
        : state(state)
        , patch(patch)
    {}

    auto changes_begin() const -> ChangesIterator {
        return patch.begin();
    }

    auto changes_end() const -> ChangesIterator {
        return patch.end();
    }

    auto changes() const -> Changes {
        return Changes{this};
    }

    auto delta() const -> DeltaView {
        return DeltaView(state, patch);
    }

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
        throw std::out_of_range("Overlay::at");
    }

    SizeType size() const override {
        SizeType result = state.size();

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

    void clear() override {
        patch.clear();
        patch.reserve(state.size());

        for (const auto entry : state) {
            patch.insert(entry.first, std::nullopt);
        }
    }

    void reserve(SizeType capacity) override {
        patch.reserve(capacity);
    }

    void insert(const Key& key, const Val& value) override {
        patch.insert(key, value);
    }

    void insert(Key&& key, Val&& value) override {
        patch.insert(std::move(key), std::move(value));
    }

    bool erase(const Key& key) override {
        const bool existed_in_state = state.contains(key);
        const auto* patched = patch.find(key);

        if (patched) {
            if (!patched->has_value()) return false;

            if (!existed_in_state) return patch.erase(key);

            patch.insert(key, std::nullopt);
            return true;
        }

        if (!existed_in_state) return false;

        patch.insert(key, std::nullopt);
        return true;
    }

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
    Patch& patch;
};

} // namespace base
