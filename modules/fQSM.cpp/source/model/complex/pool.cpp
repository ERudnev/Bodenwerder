#include <fQSM/model/complex/pool.h>

namespace fqsm::model::complex {

    namespace {
        // A line that once held this many patchlets is freed: clearing its index would cost more than it saves.
        constexpr std::size_t keepLimit = 4096;
    }

    LinePool::LinePool(Schema schema)
        : schema(schema)
        , free(schema->slotCount())
    {}

    std::unique_ptr<erased::PatchLine> LinePool::take_patch(Slot slot, bool& allocated) {
        auto& list = free[slot].patches;
        allocated = list.empty();
        if (allocated) {
            ++allocations;
            return std::make_unique<erased::PatchLine>(schema->descriptors[slot]);
        }
        auto line = std::move(list.back());
        list.pop_back();
        return line;
    }

    void LinePool::give_patch(Slot slot, std::unique_ptr<erased::PatchLine> line) {
        if (not line or line->count() > keepLimit) return;
        line->clear();
        free[slot].patches.push_back(std::move(line));
    }

    std::unique_ptr<erased::FutureLine> LinePool::take_future(Slot slot, const erased::ReadLine& base, erased::PatchLine& patch) {
        auto& list = free[slot].futures;
        if (list.empty()) {
            ++allocations;
            return std::make_unique<erased::FutureLine>(base, patch);
        }
        auto line = std::move(list.back());
        list.pop_back();
        line->bind(base, patch);
        return line;
    }

    void LinePool::give_future(Slot slot, std::unique_ptr<erased::FutureLine> line) {
        if (line) free[slot].futures.push_back(std::move(line));
    }
}
