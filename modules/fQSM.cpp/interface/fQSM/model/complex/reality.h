#pragma once

#include <memory>
#include <vector>

#include <fQSM/erased/line.h>
#include <fQSM/model/complex/state.h>

namespace fqsm::model::complex {

    // Materialized state: every slot has its line.
    class Reality : public State {
    public:
        explicit Reality(Schema schema);
        explicit Reality(const State& source);

        const erased::ReadLine& line(Slot slot) const override { return *lines[slot]; }
        erased::Line& writable(Slot slot) { return *lines[slot]; }

        const std::vector<erased::InboundIndex>* inbound() const override { return &indexes; }
        // Before a patch line is integrated into slot: the indexes of the links whose client is slot learn its changes.
        void learn(Slot slot, const erased::PatchLine& patch);
        // After a direct pass over slot: the indexes of the links whose client is slot are rebuilt from the line.
        void rebuild_inbound(Slot slot);
        void rebuild_inbound();

    protected:
        erased::Line* writable_line(Slot slot) override { return lines[slot].get(); }

    private:
        std::vector<std::unique_ptr<erased::Line>> lines;
        std::vector<erased::InboundIndex> indexes;   // by schema link
    };
}
