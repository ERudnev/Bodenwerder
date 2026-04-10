#pragma once

#include <utility>
#include <vector>

#include <iQSM/_forwards.h>
#include <iQSM/resources/layer.h>
#include <iQSM/resources/materializer.h>

namespace iqsm::resources {
    struct ManagerCore;

    struct SlotAbstract {
        virtual ~SlotAbstract() = default;
        virtual void shutdown(ref<ManagerCore>, World) noexcept = 0;
    };

    template<typename Meta>
    struct Slot final : SlotAbstract {
        Layer<Meta> layer;
        ref<resources::Materializer<Meta>> materializer;

        explicit Slot(ref<resources::Materializer<Meta>> materializer)
            : materializer(std::move(materializer)) {}

        void shutdown(ref<ManagerCore> manager, World world) noexcept override {
            std::vector<typename Meta::Id> ids;
            ids.reserve(layer.values.size());
            for (const auto& [id, _] : layer.values) {
                ids.push_back(id);
            }

            for (const auto& id : ids) {
                try {
                    materializer->release(manager, world, id);
                } catch (...) {
                }
            }
        }
    };
}
