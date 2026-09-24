#pragma once

#include <cstddef>
#include <optional>

#include <fQSM/meta/interface.include.h>
#include <base/cannonball/patch.h>
#include <base/function_ref.h>
#include <base/shared_reference.h>
#include <fQSM/erased/line.h>
#include <fQSM/model/_forwards.h>


namespace fqsm::model::linear {

    template<category::Any Meta>
    struct Patch : patch::Erased {
        // TODO: make private with const access (give monopoly to "put_.." functions?
        // or.. remove "put_..." functions :)
        // or.. make "PatchAssemblyInterface"
        base::cannonball::Patch<Id<Meta>, Quantum<Meta>> items;
        std::optional<GlobalValue<Meta>> global; // nullopt means "no change"

        Patch() = default;
        Patch(const Patch& other) : items(other.items), global(other.global) {}
        Patch& operator=(const Patch& other) {
            items = other.items;
            global = other.global;
            return *this;
        }

        bool has_changes() const override { return not items.empty() or global.has_value(); }
        void absorb(const Patch&);
        void clear();

        // Erased read access for the overlay and delta cursors.
        const erased::ReadPatch& view() const { return reader; }

        // schema
        static ref<patch::Erased> create() { return base::make_shared<Patch<Meta>>(); }

    private:
        class Reader final : public erased::ReadPatch {
        public:
            explicit Reader(const Patch& owner) : owner(owner) {}

            std::size_t count() const override { return owner.items.size(); }
            RawId id_at(std::size_t index) const override { return (*owner.items.raw_entries())[index].id.raw(); }
            erased::Mention at(std::size_t index) const override {
                const auto& patchlet = (*owner.items.raw_entries())[index].value;
                return erased::Mention{true, patchlet.tombstone, &patchlet.quantum};
            }
            erased::Mention mention(RawId id) const override {
                if (const auto* patchlet = owner.items.find(Id<Meta>{id}))
                    return erased::Mention{true, patchlet->tombstone, &patchlet->quantum};
                return {};
            }
            const void* global() const override { return owner.global ? &*owner.global : nullptr; }

        private:
            const Patch& owner;
        };

        Reader reader{*this};
    };
}

// Impl
namespace fqsm::model::linear {

    template<category::Any Meta>
    void Patch<Meta>::absorb(const Patch& other) {
        if (other.global.has_value()) global = other.global;
        base::cannonball::Patch<Id<Meta>, Quantum<Meta>>::merge(items, other.items);
    }

    template<category::Any Meta>
    void Patch<Meta>::clear() {
        items.clear();
        global.reset();
    }

}
