#include <fQSM/processing/actions/normalization.h>

#include <format>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

#include <fQSM/processing/actions/integration.h>
#include <fQSM/processing/actions/merge.h>
#include <fQSM/processing/review.h>
#include <fQSM/state/world/preview.h>
#include <fQSM/state/details/analysis.h>
#include <fQSM/state/patch.h>
#include <fQSM/features/reaction.h>

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

    void append(Review::Notes& dst, Review::Notes src) {
        dst.critical.insert(dst.critical.end(), src.critical.begin(), src.critical.end());
        dst.warning.insert(dst.warning.end(), src.warning.begin(), src.warning.end());
    }

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

    // build one normalization wave
    auto normalizer(Reading source, const Patch& patch) -> actions::NormalizationResult {
        Review::Notes notes;
        auto review = Reviewing{
            state::world::Preview{source, patch},
            ::base::make_shared<Patch>(patch.schema),
            notes,
        };

        std::set<schema::Dag::ReactionId> selectedReactions;
        for (const auto& [sourceType, _] : patch.composite().slices) {
            const auto found = patch.schema->nodes.find(sourceType);
            if (found == patch.schema->nodes.end()) continue;

            for (const auto reactionId : found->second.reactions) {
                selectedReactions.insert(reactionId);
            }
        }

        for (const auto reactionId : selectedReactions) {
            patch.schema->reactions.at(reactionId.raw())->apply(review);
        }

        return { review.patch, std::move(notes) };
    }


    auto normalize_recursive(Reading source, PatchRef accumulated, UpdateControl& control) -> Review::Notes {
        control.statistics.emplace_back(*accumulated);

        const auto fix = normalizer(source, *accumulated);
        auto notes = fix.notes;
        if (notes.rejection()) return notes;

        const auto fixStats = analysis::Patch{*fix.patch};
        if (fixStats.overallChanges() == 0) return notes;

        if (is_suspicious_growth(control)) return notes;

        ensure_depth_limit(control);
        ++control.counter;
        actions::merge(source, accumulated, fix.patch);
        append(notes, normalize_recursive(source, accumulated, control));
        return notes;
    }
}


// facade part
namespace fqsm::processing::actions {

    auto normalize(Reading source, const Patch& patch) -> NormalizationResult {
        auto normalized = ::base::make_shared<Patch>(patch);
        normalization::UpdateControl control{temp_defence_normalization_waves};
        auto notes = normalization::normalize_recursive(source, normalized, control);
        return { normalized, std::move(notes) };
    }

    auto update(State& state, const Patch& patch) -> Review::Notes {
        const auto normalized = normalize(state, patch);
        if (normalized.notes.rejection()) return normalized.notes;

        integrate(state, *normalized.patch);
        return normalized.notes;
    }
}