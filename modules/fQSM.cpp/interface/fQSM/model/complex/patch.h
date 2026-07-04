#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/linear/patch.h>
#include <fQSM/model/intertype/composite.h>

namespace fqsm::model::complex {

    struct WorkersInterface {
        WorkersInterface(Schema schema, intertype::Composite<linear::patch::Erased> builtLines) : schema(schema), lines(std::move(builtLines)) {}

        template<category::Any Meta>
        linear::WorkersInterface<Meta>& updates();

        template<category::Any Meta>
        const linear::WorkersInterface<Meta>& updates() const;
    protected:
        const Schema schema;
        const intertype::Composite<linear::patch::Erased> lines;
    };

    struct Patch : WorkersInterface {
        using WorkersInterface::schema;
        using WorkersInterface::lines;

        Patch(Schema schema) : WorkersInterface(schema, composition(schema)) {}

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

    template<category::Any Meta>
    linear::WorkersInterface<Meta>& WorkersInterface::updates() {
        return static_cast<linear::Patch<Meta>&>(*lines.container.at(TypeId<Meta>).get());
    };

    template<category::Any Meta>
    const linear::WorkersInterface<Meta>& WorkersInterface::updates() const {
        return static_cast<const linear::Patch<Meta>&>(*lines.container.at(TypeId<Meta>).get());
    }
}
