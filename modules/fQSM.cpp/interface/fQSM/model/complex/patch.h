#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <fQSM/erased/patch_line.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/intertype/schema.h>

namespace fqsm::model::complex {

    // Changes for a complex state: one patch line per slot, created on first write.
    struct Patch {
        using Slot = intertype::Graph::Slot;

        explicit Patch(Schema schema);
        Patch(const Patch& other);
        Patch& operator=(const Patch&) = delete;

        struct Summary {
            using Category = std::vector<std::string>;
            Category critical;
            Category warning;

            bool good() const { return critical.empty(); }
        };

        // nullptr when the slot was never written
        const erased::PatchLine* line(Slot slot) const { return lines[slot].get(); }
        erased::PatchLine& writable(Slot slot);

        bool has_changes() const;
        void absorb(const Patch&);
        void clear();

        // number of patch lines created so far (lazy creation is observable for tests)
        std::size_t linesCreated() const { return created; }

        // public.. still. sonsider to make write-only for workers
        Summary summary;
        const Schema schema;

    private:
        std::vector<std::unique_ptr<erased::PatchLine>> lines;
        std::size_t created = 0;
    };
}
