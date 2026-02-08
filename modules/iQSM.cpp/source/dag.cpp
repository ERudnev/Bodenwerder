#include <iQSM/dag.h>

#include <stdexcept>

namespace iqsm {
    void DagState::check_closed() const {
        for (const auto& [a, e] : aspects) {
            for (const auto& dep : e.depends_from) {
                if (aspects.find(dep) == aspects.end()) {
                    throw std::runtime_error("DagState is not closed");
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


