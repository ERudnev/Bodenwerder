#pragma once

#include <cstddef>

#include <fQSM/erased/line.h>
#include <fQSM/erased/patch_line.h>

namespace fqsm::erased {

    // A line seen through one patch: reads resolve the patch first (a tombstone hides the id), then base.
    // The base may itself be a FutureLine; iteration then stacks one more cursor layer.
    // Writes go to the patch only.
    class FutureLine final : public ReadLine {
    public:
        FutureLine(const ReadLine& base, PatchLine& patch) : below(&base), layer(&patch) {}
        void bind(const ReadLine& base, PatchLine& patch) { below = &base; layer = &patch; }

        const ReadLine& base() const { return *below; }
        const PatchLine& patch() const { return *layer; }

        bool contains(RawId id) const override;
        const void* find(RawId id) const override;
        std::size_t size() const override;
        const void* global() const override;
        Cursor cursor_begin() const override;
        Cursor cursor_end() const override;
        const Ops& quantum_ops() const override { return layer->quantum_ops(); }

        // value is moved from
        void put_modification(RawId id, void* value);
        void put_add(RawId id, void* value);
        void put_global(const void* value);
        // Tombstone with the last visible value; no-op when neither the patch nor the base has id.
        void put_deletion(RawId id);
        // Touch: an existing patchlet (tombstoned or not) is reused; else an unverified copy of the
        // base value is made. nullptr when neither the patch nor the base has id.
        void* get_modification_access(RawId id);
        void* get_access_global();

    private:
        const ReadLine* below;
        PatchLine* layer;
    };
}
