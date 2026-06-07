#include <fQSM/processing/actions/normalization.h>

#include <format>
#include <stdexcept>
#include <vector>

#include <fQSM/processing/actions/integration.h>
#include <fQSM/processing/review.h>
#include <fQSM/state/world/preview.h>
#include <fQSM/state/details/analysis.h>
#include <fQSM/state/patch.h>

// local alias:
namespace fqsm::processing::actions {
    static constexpr int temp_defence_normalization_waves = 10;

    using State = fqsm::state::world::Data;
    using Patch = fqsm::state::world::Patch;
    using PatchRef = fqsm::ref<Patch>;
}

// internal part of normalization
namespace fqsm::processing::actions::normalization {

    struct UpdateControl {
        const int limit;
        int counter = 0;
        std::vector<analysis::Patch> statistics;
    };

    constexpr std::size_t early_window = 4;

    auto is_suspicious_growth(const UpdateControl& control) -> bool {
        if (control.statistics.size() < early_window) return false;

        const auto offset = control.statistics.size() - early_window;
        int previous = control.statistics[offset].overallChanges();
        if (previous == 0) return false;

        for (std::size_t i = offset + 1; i < control.statistics.size(); ++i) {
            const int current = control.statistics[i].overallChanges();
            if (current <= previous) return false;
            previous = current;
        }
        return true;
    }

    void ensure_depth_limit(const UpdateControl& control) {
        if (control.counter < control.limit) return;

        throw std::runtime_error(std::format("normalization: depth limit {} reached", control.limit));
    }

    // build normalization patch
    PatchRef normalizer(const State& state, const Patch& patch) {
        auto review = Reviewing{
            state::world::Preview{state, patch},
            base::make_shared<Patch>(patch.schema),
        };

        for (const auto& [aspectId, node] : patch.schema->nodes) {
            // TODO: codex access should come from schema/binding side, not from a parallel registry here.
        }

        return review.patch;
    }


    void update_recursive(State& state, const Patch& patch, UpdateControl& control) {
        control.statistics.emplace_back(patch);

        const auto fix = normalizer(state, patch);
        integrate(state, patch);

        const auto fixStats = analysis::Patch{*fix};
        if (fixStats.overallChanges() == 0) return;

        if (is_suspicious_growth(control)) return;

        ensure_depth_limit(control);
        ++control.counter;
        update_recursive(state, *fix, control);
    }
}


// facade part
namespace fqsm::processing::actions {

    void update(State& state, const Patch& patch) {
        normalization::UpdateControl control{temp_defence_normalization_waves};
        normalization::update_recursive(state, patch, control);
    }
}