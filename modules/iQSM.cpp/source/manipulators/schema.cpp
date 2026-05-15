#include <iQSM/manipulators/schema.h>

#include <stdexcept>

#include <iQSM/state/schema.h>

namespace {
    template<typename Fn>
    void for_each_aspect(const iqsm::state::SchemaData& schema, Fn&& fn) {
        for (const auto& [type, aspect] : schema.versioned) {
            fn(type, aspect);
        }

        for (const auto& [type, aspect] : schema.operational) {
            fn(type, aspect);
        }
    }

    template<typename Fn>
    void for_each_aspect(iqsm::state::SchemaData& schema, Fn&& fn) {
        for (auto& [type, aspect] : schema.versioned) {
            fn(type, aspect);
        }

        for (auto& [type, aspect] : schema.operational) {
            fn(type, aspect);
        }
    }

    auto contains_aspect(const iqsm::state::SchemaData& schema, iqsm::RAId type) -> bool {
        return schema.versioned.contains(type) || schema.operational.contains(type);
    }

    template<typename Fn>
    void with_aspect(iqsm::state::SchemaData& schema, iqsm::RAId type, Fn&& fn) {
        if (auto it = schema.versioned.find(type); it != schema.versioned.end()) {
            fn(it->second);
            return;
        }

        fn(schema.operational.at(type));
    }

    auto is_closed(const iqsm::state::SchemaData& schema) -> bool {
        bool closed = true;
        for_each_aspect(schema, [&](iqsm::RAId, const auto& aspect) {
            for (const auto& dep : aspect.requiredByMe) {
                if (!contains_aspect(schema, dep)) {
                    closed = false;
                    return;
                }
            }
        });
        return closed;
    }

    void rebuild_required_by(iqsm::state::SchemaData& schema) {
        for_each_aspect(schema, [](iqsm::RAId, auto& aspect) {
            aspect.requiredBy.clear();
        });

        for_each_aspect(schema, [&](iqsm::RAId type, const auto& aspect) {
            for (const auto& dep : aspect.requiredByMe) {
                with_aspect(schema, dep, [&](auto& dep_aspect) {
                    dep_aspect.requiredBy.insert(type);
                });
            }
        });
    }

    void validate_layer_dependencies(const iqsm::state::SchemaData& schema) {
        for (const auto& [_, aspect] : schema.operational) {
            for (const auto& dep : aspect.requiredByMe) {
                if (schema.versioned.contains(dep)) {
                    throw std::runtime_error("state::schema::merge(): operational aspect cannot depend on versioned aspect");
                }
            }
        }
    }

    template<typename Aspect>
    void merge_aspect(Aspect& accumulated, const Aspect& incoming) {
        if (accumulated.layer != incoming.layer) {
            throw std::runtime_error("state::schema::merge(): same aspect inserted with different layer axis");
        }

        if (accumulated.name.empty()) accumulated.name = incoming.name;
        else if (!incoming.name.empty() && accumulated.name != incoming.name) {
            throw std::runtime_error("state::schema::merge(): same aspect inserted with different names");
        }

        accumulated.requiredByMe.insert(incoming.requiredByMe.begin(), incoming.requiredByMe.end());
    }

    template<typename AspectMap>
    void merge_layer(AspectMap& accumulated, const AspectMap& incoming, const auto& opposite_layer) {
        for (const auto& [type, aspect] : incoming) {
            if (opposite_layer.contains(type)) {
                throw std::runtime_error("state::schema::merge(): same aspect inserted with different layer axis");
            }

            const auto [it, inserted] = accumulated.emplace(type, aspect);
            if (!inserted) {
                merge_aspect(it->second, aspect);
            }
        }
    }
}

namespace iqsm::manipulator::schema {
    iqsm::Schema merge(std::initializer_list<iqsm::Schema> parts) {
        auto out = base::make_shared<iqsm::state::SchemaData>();

        for (const auto& part : parts) {
            merge_layer(out->versioned, part->versioned, out->operational);
            merge_layer(out->operational, part->operational, out->versioned);
        }

        if (!is_closed(*out)) {
            throw std::runtime_error("state::schema::merge(): schema is not closed");
        }

        validate_layer_dependencies(*out);
        rebuild_required_by(*out);
        return iqsm::freeze(out);
    }
}