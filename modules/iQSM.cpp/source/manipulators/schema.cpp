#include <iQSM/manipulators/schema.h>

#include <iQSM/state/schema.h>

namespace iqsm::manipulator::schema {
    iqsm::Schema merge(std::initializer_list<iqsm::Schema> parts) {
        auto out = base::make_shared<iqsm::state::SchemaData>();

        for (const auto& part : parts) {
            for (const auto& [type, aspect] : part->aspects) {
                out->aspects.emplace(type, aspect);
            }
        }

        return iqsm::freeze(out);
    }
}