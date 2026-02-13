#include <iQSM/schema.h>

#include <format>
#include <stdexcept>

namespace iqsm {
    void SchemaObject::check_closed() const {
        for (const auto& [a, e] : aspects) {
            for (const auto& dep : e.require) {
                if (aspects.find(dep) == aspects.end()) {
                    throw std::runtime_error(std::format(
                        "Schema is not closed: missing dependency '{}' (required by '{}')",
                        dep.name(),
                        a.name()));
                }
            }
        }
    }

    void SchemaObject::update_required_by() {
        for (auto& [_, e] : aspects) { e.required_by.clear(); }
        for (const auto& [a, e] : aspects) {
            for (const auto& dep : e.require) {
                aspects.at(dep).required_by.insert(a);
            }
        }
    }
}


