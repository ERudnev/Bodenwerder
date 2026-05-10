#include <iQSM/manipulators/schema.h>

#include <stdexcept>

#include <iQSM/state/schema.h>

namespace {
    auto is_closed(const iqsm::state::SchemaData& schema) -> bool {
        for (const auto& [_, aspect] : schema.aspects) {
            for (const auto& dep : aspect.requiredByMe) {
                if (!schema.aspects.contains(dep)) return false;
            }
        }
        return true;
    }

    void rebuild_required_by(iqsm::state::SchemaData& schema) {
        for (auto& [_, aspect] : schema.aspects) {
            aspect.requiredBy.clear();
        }

        for (const auto& [type, aspect] : schema.aspects) {
            for (const auto& dep : aspect.requiredByMe) {
                schema.aspects.at(dep).requiredBy.insert(type);
            }
        }
    }

    void validate_layer_dependencies(const iqsm::state::SchemaData& schema) {
        for (const auto& [_, aspect] : schema.aspects) {
            if (aspect.layer != iqsm::state::policy::versioning::single) continue;

            for (const auto& dep : aspect.requiredByMe) {
                if (schema.aspects.at(dep).layer == iqsm::state::policy::versioning::shared) {
                    throw std::runtime_error("state::schema::merge(): operational aspect cannot depend on versioned aspect");
                }
            }
        }
    }

    void merge_aspect(iqsm::state::SchemaData::Aspect& accumulated, const iqsm::state::SchemaData::Aspect& incoming) {
        if (accumulated.layer != incoming.layer) {
            throw std::runtime_error("state::schema::merge(): same aspect inserted with different layer policy");
        }

        if (accumulated.name.empty()) accumulated.name = incoming.name;
        else if (!incoming.name.empty() && accumulated.name != incoming.name) {
            throw std::runtime_error("state::schema::merge(): same aspect inserted with different names");
        }

        accumulated.requiredByMe.insert(incoming.requiredByMe.begin(), incoming.requiredByMe.end());
    }
}

namespace iqsm::manipulator::schema {
    iqsm::Schema merge(std::initializer_list<iqsm::Schema> parts) {
        auto out = base::make_shared<iqsm::state::SchemaData>();

        for (const auto& part : parts) {
            for (const auto& [type, aspect] : part->aspects) {
                const auto [it, inserted] = out->aspects.emplace(type, aspect);
                if (!inserted) merge_aspect(it->second, aspect);
            }
        }

        if (!is_closed(*out)) {
            throw std::runtime_error("state::schema::merge(): schema is not closed");
        }

        validate_layer_dependencies(*out);
        rebuild_required_by(*out);
        return iqsm::freeze(out);
    }
}