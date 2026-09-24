#include <fQSM/model/complex/patch.h>

#include <fQSM/model/complex/pool.h>
#include <fQSM/model/complex/state.h>

namespace fqsm::model::complex {

    Patch::Patch(Schema schema, std::shared_ptr<LinePool> pool)
        : summary()
        , schema(schema)
        , pool(std::move(pool))
        , lines(schema->slotCount())
    {}

    Patch::Patch(const State& over)
        : Patch(over.schema, over.linePool())
    {}

    Patch::~Patch() {
        if (not pool) return;
        for (Slot slot = 0; slot < lines.size(); ++slot)
            pool->give_patch(slot, std::move(lines[slot]));
    }

    Patch::Patch(const Patch& other)
        : summary(other.summary)
        , schema(other.schema)
        , pool(other.pool)
        , lines(other.lines.size())
    {
        for (std::size_t slot = 0; slot < lines.size(); ++slot) {
            if (not other.lines[slot]) continue;
            lines[slot] = std::make_unique<erased::PatchLine>(*other.lines[slot]);
            ++created;
            ++allocated;
        }
    }

    erased::PatchLine& Patch::writable(Slot slot) {
        auto& line = lines[slot];
        if (not line) {
            bool fresh = true;
            line = pool ? pool->take_patch(slot, fresh) : std::make_unique<erased::PatchLine>(schema->descriptors[slot]);
            ++created;
            if (fresh) ++allocated;
        }
        return *line;
    }

    bool Patch::has_changes() const {
        for (const auto& line : lines)
            if (line and line->has_changes())
                return true;
        return false;
    }

    void Patch::absorb(const Patch& other) {
        for (Slot slot = 0; slot < other.lines.size(); ++slot) {
            const auto& source = other.lines[slot];
            if (not source or not source->has_changes()) continue;
            writable(slot).absorb(*source);
        }

        summary.critical.insert(summary.critical.end(), other.summary.critical.begin(), other.summary.critical.end());
        summary.warning.insert(summary.warning.end(), other.summary.warning.begin(), other.summary.warning.end());
    }

    void Patch::clear() {
        for (const auto& line : lines)
            if (line) line->clear();
        summary = {};
    }

}
