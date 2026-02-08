#include <iQSM/dag.h>

#include <format>
#include <stdexcept>

namespace iqsm {
    void DagState::check_closed() const {
        for (const auto& [a, e] : aspects) {
            for (const auto& dep : e.depends_from) {
                if (aspects.find(dep) == aspects.end()) {
                    throw std::runtime_error(std::format(
                        "DagState is not closed: missing dependency '{}' (required by '{}')",
                        dep.name(),
                        a.name()));
                }
            }
        }
    }

    void DagState::update_dependents() {
        for (auto& [_, e] : aspects) { e.they_depend.clear(); }
        for (const auto& [a, e] : aspects) {
            for (const auto& dep : e.depends_from) {
                aspects.at(dep).they_depend.insert(a);
            }
        }
    }
}


