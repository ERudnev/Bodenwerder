#include <fQSM/processing/actions/normalization.h>

#include <format>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

#include <fQSM/processing/actions/integration.h>
#include <fQSM/processing/actions/merge.h>
#include <fQSM/processing/review.h>
#include <fQSM/model/analysis.h>
#include <fQSM/model/complex/draft.h>
#include <fQSM/model/structure/schema.h>
#include <fQSM/features/reaction.h>

// local alias:
namespace fqsm::processing::actions {
    static constexpr int temp_defence_normalization_waves = 10;

    using Patch = fqsm::model::complex::Patch;
    using PatchRef = fqsm::ref<Patch>;
}

// internal part of normalization
namespace fqsm::processing::actions::normalization {

    struct UpdateControl {
        const int limit;
        int counter = 0;
        std::vector<analysis::Patch> statistics;
    };

    void append(review::Notes& dst, review::Notes src) {
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
    auto normalizer(Reading source, fqsm::cref<Patch> patch, const Rtid::Set& taintedLines) -> actions::NormalizationResult {
        review::Notes notes;

        auto correctionsPatch = base::make_shared<Patch>(source.schema);

        // this is very important place: this cast is saves about ~300 lines of code for new class
        // complex::Proposal === const complex::Draft
        fqsm::ref<Patch> non_const_patch(std::const_pointer_cast<Patch>(patch.std_ptr()));

        auto review = Review(model::complex::Draft{source, non_const_patch, taintedLines}, correctionsPatch, notes);

        std::set<model::structure::AspectGraph::ReactionId> selectedReactions;
        for (const auto& [sourceType, _] : patch->lines.container) {
            const auto found = patch->schema->nodes.find(sourceType);
            if (found == patch->schema->nodes.end()) continue;

            for (const auto reactionId : found->second.reactions) {
                selectedReactions.insert(reactionId);
            }
        }

        for (const auto reactionId : selectedReactions) {
            patch->schema->reactions.at(reactionId.raw())->apply(review);
        }

        // TODO: allow Reviewers to call Immediate/Direct contexts and collect new tainted flags
        const Rtid::Set fakeModifiedTaintFlags{}; // will be populated with taints from Review process

        return { correctionsPatch, std::move(fakeModifiedTaintFlags), std::move(notes) };
    }


    auto normalize_recursive(Reading source, PatchRef accumulated, Rtid::Set taintedLines, UpdateControl& control) -> review::Notes {
        control.statistics.emplace_back(*accumulated);

        const auto fix = normalizer(source, accumulated, taintedLines);
        auto notes = fix.notes;
        if (notes.rejection()) return notes;

        const auto fixStats = analysis::Patch{*fix.patch};
        if (fixStats.overallChanges() == 0 and fix.taintedDuringPatch.empty() ) return notes;

        if (is_suspicious_growth(control)) return notes;

        ensure_depth_limit(control);
        ++control.counter;
        actions::merge(source, accumulated, fix.patch);
        append(notes, normalize_recursive(source, accumulated, fix.taintedDuringPatch, control));
        return notes;
    }
}


// facade part
namespace fqsm::processing::actions {

    auto normalize(Reading source, const Patch& patch, Rtid::Set taintedLines) -> NormalizationResult {
        auto normalized = base::make_shared<Patch>(patch);
        normalization::UpdateControl control{temp_defence_normalization_waves};
        auto notes = normalization::normalize_recursive(source, normalized, taintedLines, control);
        return { normalized, {}, std::move(notes) };
    }

    auto update(model::complex::Reality& state, const Patch& patch, Rtid::Set taintedLines) -> review::Notes {
        const auto normalized = normalize(state, patch, taintedLines);
        if (normalized.notes.rejection()) return normalized.notes;

        integrate(state, *normalized.patch);
        return normalized.notes;
    }
}
