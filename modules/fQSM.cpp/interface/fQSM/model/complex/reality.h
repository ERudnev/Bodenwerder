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

    protected:
        erased::Line* writable_line(Slot slot) override { return lines[slot].get(); }

    private:
        std::vector<std::unique_ptr<erased::Line>> lines;
    };
}
