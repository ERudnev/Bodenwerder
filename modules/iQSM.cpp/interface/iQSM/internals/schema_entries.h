#pragma once

#include <memory>
#include <optional>

#include <iQSM/references.h>

namespace iqsm { struct FieldAbstract; }
namespace iqsm::delta { struct FieldDiffAbstract; }
namespace iqsm::resources { struct SlotAbstract; }

namespace iqsm::internals::schema {
    // Schema-owned typed “slots” for per-aspect operations/data.
    // These are intentionally type-erased and rely on forward declarations to keep `schema.h` light.

    struct FieldEntry {
        cref<FieldAbstract> zero;

        template<meta::Aspect Meta>
        static FieldEntry make();
    };

    struct DeltaEntry {
        using UField = ref<::iqsm::delta::FieldDiffAbstract>;

        using MakeDeltaField = std::optional<UField> (*)(cref<FieldAbstract> from, cref<FieldAbstract> to);
        using IntegrateField = cref<FieldAbstract> (*)(cref<FieldAbstract> current, UField diff);
        using Empty = bool (*)(UField diff);
        using Absorb = void (*)(UField lhs, UField rhs);

        MakeDeltaField make_delta_field = nullptr;
        IntegrateField integrate_field = nullptr;
        Empty empty = nullptr;
        Absorb absorb = nullptr;

        template<meta::Aspect Meta>
        static DeltaEntry make();
    };

    struct ResourceEntry {
        using CreateSlot = std::unique_ptr<resources::SlotAbstract> (*)();

        CreateSlot create_slot = nullptr;

        template<meta::Aspect Meta>
        static ResourceEntry make();
    };
}

