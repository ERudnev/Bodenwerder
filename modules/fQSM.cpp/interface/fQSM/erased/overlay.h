#pragma once

#include <cstddef>

#include <fQSM/erased/line.h>
#include <fQSM/erased/patch_line.h>

namespace fqsm::erased {

    // A line seen through one patch layer: reads resolve the patch first (a tombstone hides the id), then base.
    // The base may itself be an Overlay; iteration then stacks one more cursor layer.
    class Overlay final : public ReadLine {
    public:
        Overlay(const ReadLine& base, const PatchLine& patch) : below(&base), layer(&patch) {}

        const ReadLine& base() const { return *below; }
        const PatchLine& patch() const { return *layer; }

        bool contains(RawId id) const override;
        const void* find(RawId id) const override;
        std::size_t size() const override;
        const void* global() const override;
        Cursor cursor_begin() const override;
        Cursor cursor_end() const override;

    private:
        const ReadLine* below;
        const PatchLine* layer;
    };
}
