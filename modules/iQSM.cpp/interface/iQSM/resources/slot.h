#pragma once

#include <utility>

#include <iQSM/resources/layer.h>
#include <iQSM/resources/materializer.h>

namespace iqsm::resources {
    struct SlotAbstract {
        virtual ~SlotAbstract() = default;
    };

    template<typename Meta>
    struct Slot final : SlotAbstract {
        Layer<Meta> layer;
        ref<resources::Materializer<Meta>> materializer;

        explicit Slot(ref<resources::Materializer<Meta>> materializer)
            : materializer(std::move(materializer)) {}
    };
}
