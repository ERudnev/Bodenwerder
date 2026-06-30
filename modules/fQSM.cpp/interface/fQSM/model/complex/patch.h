#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/linear/patch.h>
#include <fQSM/model/structure/composite.h>

namespace fqsm::model::complex {

    struct Patch {
        const Schema schema;
        const Composite<linear::patch::Erased> lines;

        Patch(Schema schema) : schema(schema), lines(composition(schema)) {}

        template<category::Any Meta>
        linear::Patch<Meta>& aspect();

        template<category::Any Meta>
        const linear::Patch<Meta>& aspect() const;

        std::size_t quanta() const;
        void absorb(const Patch&);
        void clear();

    private:
        static Composite<linear::patch::Erased> composition(Schema);
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
