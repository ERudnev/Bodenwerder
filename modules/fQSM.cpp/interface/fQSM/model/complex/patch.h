#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/linear/patch.h>
#include <fQSM/model/intertype/composite.h>

namespace fqsm::model::complex {

    struct Patch {
        Patch(Schema schema) : summary(), schema(schema), lines(composition(schema)) {}

        struct Summary {
            using Category = std::vector<std::string>;
            Category critical;
            Category warning;

            bool good() const { return critical.empty(); }
        };

        template<category::Any Meta>
        linear::Patch<Meta>& aspect();

        template<category::Any Meta>
        const linear::Patch<Meta>& aspect() const;

        bool has_changes() const;
        void absorb(const Patch&);
        void clear();

        // schema
        template<category::Any Meta>
        static void absorb(Patch& target, const Patch& source) {
            target.aspect<Meta>().absorb(source.aspect<Meta>());
        }

        template<category::Any Meta>
        static void clear(Patch& patch) {
            patch.aspect<Meta>().clear();
        }

        // public.. still. sonsider to make write-only for workers
        Summary summary;
        const Schema schema;
        const intertype::Composite<linear::patch::Erased> lines;

    private:
        static intertype::Composite<linear::patch::Erased> composition(Schema);
    };
}

namespace fqsm::model::complex {

    template<category::Any Meta>
    linear::Patch<Meta>& Patch::aspect() {
        return static_cast<linear::Patch<Meta>&>(*lines.container.at(TypeId<Meta>).get());
    };

    template<category::Any Meta>
    const linear::Patch<Meta>& Patch::aspect() const {
        return static_cast<const linear::Patch<Meta>&>(*lines.container.at(TypeId<Meta>).get());
    }
}
