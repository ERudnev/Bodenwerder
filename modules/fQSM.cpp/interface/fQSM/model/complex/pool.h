#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <fQSM/erased/future_line.h>
#include <fQSM/erased/patch_line.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/intertype/schema.h>

namespace fqsm::model::complex {

    // Per-Realm free lists of patch lines and future lines, by slot, so that
    // transactions reuse them instead of allocating. Single-threaded, like the Realm that owns it.
    class LinePool {
    public:
        using Slot = intertype::Graph::Slot;

        explicit LinePool(Schema schema);
        LinePool(const LinePool&) = delete;
        LinePool& operator=(const LinePool&) = delete;

        // allocated is set when no pooled line was available
        std::unique_ptr<erased::PatchLine> take_patch(Slot slot, bool& allocated);
        // Clears the line; lines that grew large are freed instead of kept.
        void give_patch(Slot slot, std::unique_ptr<erased::PatchLine> line);

        std::unique_ptr<erased::FutureLine> take_future(Slot slot, const erased::ReadLine& base, erased::PatchLine& patch);
        void give_future(Slot slot, std::unique_ptr<erased::FutureLine> line);

        // total objects allocated by this pool (patch lines, future lines)
        std::size_t allocated() const { return allocations; }

    private:
        struct Free {
            std::vector<std::unique_ptr<erased::PatchLine>> patches;
            std::vector<std::unique_ptr<erased::FutureLine>> futures;
        };

        const Schema schema;
        std::vector<Free> free;
        std::size_t allocations = 0;
    };
}
