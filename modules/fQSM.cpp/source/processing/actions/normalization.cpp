#include <fQSM/processing/actions/normalization.h>

#include <format>
#include <set>
#include <utility>
#include <vector>

#include <fQSM/processing/actions/integration.h>
#include <fQSM/processing/actions/merge.h>
#include <fQSM/processing/review.h>
#include <fQSM/model/analysis.h>
#include <fQSM/model/complex/future.h>
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

    struct WaveRecord {
        analysis::Patch incoming;
        analysis::Patch corrections;
    };

    struct UpdateControl {
        const int limit;
        int counter = 0;
        std::vector<WaveRecord> statistics;
    };

    void record_wave(UpdateControl& control, const Patch& incoming, const Patch& corrections) {
        control.statistics.push_back({analysis::Patch{incoming}, analysis::Patch{corrections}});
    }

    void append(review::Notes& dst, review::Notes src) {
        dst.critical.insert(dst.critical.end(), src.critical.begin(), src.critical.end());
        dst.warning.insert(dst.warning.end(), src.warning.begin(), src.warning.end());
    }

    constexpr std::size_t early_window = 4;

    auto monotonic_increase(const std::vector<int>& series) -> bool {
        if (series.size() < early_window) return false;

        const auto offset = series.size() - early_window;
        int previous = series[offset];
        if (previous == 0) return false;

        for (std::size_t i = offset + 1; i < series.size(); ++i) {
            const int current = series[i];
            if (current <= previous) return false;
            previous = current;
        }
        return true;
    }

    auto series_from_incoming(const UpdateControl& control) -> std::vector<int> {
        std::vector<int> series;
        series.reserve(control.statistics.size());
        for (const auto& wave : control.statistics)
            series.push_back(wave.incoming.overall.patchlets);
        return series;
    }

    auto series_from_corrections(const UpdateControl& control) -> std::vector<int> {
        std::vector<int> series;
        series.reserve(control.statistics.size());
        for (const auto& wave : control.statistics)
            series.push_back(wave.corrections.overall.patchlets);
        return series;
    }

    // Old model: accumulated P grows every wave.
    auto is_suspicious_incoming_growth(const UpdateControl& control) -> bool {
        return monotonic_increase(series_from_incoming(control));
    }

    // Runaway reactions: K grows every wave even if incoming shrinks.
    auto is_suspicious_corrections_growth(const UpdateControl& control) -> bool {
        return monotonic_increase(series_from_corrections(control));
    }

    // Stuck loop: same non-zero K for several waves (decaying incoming won't trigger growth checks).
    auto is_stagnant_corrections(const UpdateControl& control, std::size_t window = early_window) -> bool {
        if (control.statistics.size() < window) return false;

        const auto offset = control.statistics.size() - window;
        const int plateau = control.statistics[offset].corrections.overall.patchlets;
        if (plateau == 0) return false;

        for (std::size_t i = offset + 1; i < control.statistics.size(); ++i) {
            if (control.statistics[i].corrections.overall.patchlets != plateau)
                return false;
        }
        return true;
    }

    // Deprecated alias: was tied to accumulated incoming size.
    auto is_suspicious_growth(const UpdateControl& control) -> bool {
        return is_suspicious_incoming_growth(control);
    }

    struct PassResult {
        fqsm::ref<model::complex::Patch> patch;
        Rtid::Set taintedDuringPatch;
        review::Notes notes;
    };

    // build one normalization wave
    auto reactions_pass(Reading source, fqsm::cref<Patch> changes, const Rtid::Set& taintedLines) -> PassResult {
        base::message("creating review context");
        PassResult result{
            base::make_shared<Patch>(source.schema),
            {}, // TODO: consider filling Tainted Flags once Reviewers will become context::Direct<T> compatible
            {}
        };

        // this is very important place: this cast is saves about ~300 lines of code for new class
        // complex::Proposal === const complex::Draft
        fqsm::ref<Patch> non_const_patch(std::const_pointer_cast<Patch>(changes.std_ptr()));
        const auto proposal = model::complex::Future{source, non_const_patch, base::cannonball::SeeChanges::observable, taintedLines};
        auto context = Review(
            proposal,
            result.patch,
            result.notes);

        std::set<model::structure::AspectGraph::ReactionId> selectedReactions;
        for (const auto& [sourceType, _] : changes->lines.container) {
            const auto found = changes->schema->nodes.find(sourceType);
            if (found == changes->schema->nodes.end()) continue;

            for (const auto reactionId : found->second.reactions) {
                selectedReactions.insert(reactionId);
            }
        }

        for (const auto reactionId : selectedReactions) {
            changes->schema->reactions.at(reactionId.raw())->apply(context);
        }

        return result;
    }

    auto normalize_recursive(model::complex::Reality& state, fqsm::cref<Patch> changes, Rtid::Set taintedLines, UpdateControl& control) -> review::Notes {
        if (control.counter >= control.limit) {
            review::Notes notes;
            notes.critical.push_back(std::format("normalization: depth limit {} reached", control.limit));
            return notes;
        }

        ++control.counter;

        // call reactions pass and exit if failed (reactions may declare reviewed changes "unfixable" sometimes
        const auto reactions = reactions_pass(state, changes, taintedLines);
        record_wave(control, *changes, *reactions.patch);
        if (reactions.notes.rejection()) return reactions.notes;

        // once reactions formed, delta(state, changes) is needed by noote and state may be advanced
        integrate(state, *changes);

        // zero update is only good exit from recursion;
        const auto fixStats = analysis::Patch{*reactions.patch};
        if (fixStats.overall.patchlets == 0 and reactions.taintedDuringPatch.empty() )
            return reactions.notes;

        if (is_stagnant_corrections(control)) {
            auto notes = reactions.notes;
            notes.critical.push_back(std::format("normalization: stagnant corrections after {} passes", control.counter));
            return notes;
        }
        if (is_suspicious_corrections_growth(control)) {
            auto notes = reactions.notes;
            notes.critical.push_back(std::format("normalization: corrections growth after {} passes", control.counter));
            return notes;
        }

        auto notes = reactions.notes;
        append(notes, normalize_recursive(state, reactions.patch, reactions.taintedDuringPatch, control));
        return notes;
    }

    //auto core(model::complex::Reality& initial, const Patch& incoming
}


// facade part
namespace fqsm::processing::actions {

    auto update(model::complex::Reality& state, fqsm::cref<Patch> patch, Rtid::Set taintedLines) -> review::Notes {
        normalization::UpdateControl control{temp_defence_normalization_waves};
        return normalization::normalize_recursive(state, patch, taintedLines, control);
    }
}
