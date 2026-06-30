#include <fQSM/processing/actions/normalization.h>

#include <format>
#include <set>

#include <fQSM/processing/actions/integration.h>
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

    void append(review::Notes& dst, review::Notes src) {
        dst.critical.insert(dst.critical.end(), src.critical.begin(), src.critical.end());
        dst.warning.insert(dst.warning.end(), src.warning.begin(), src.warning.end());
    }

    struct PassResult {
        fqsm::ref<model::complex::Patch> patch;
        Rtid::Set taintedDuringPatch;
        review::Notes notes;
    };

    // build one normalization wave
    auto reactions_pass(Reading source, fqsm::cref<Patch> changes, const Rtid::Set& taintedLines) -> PassResult {
        //base::message("creating review context");
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

    review::Notes normalization(const model::complex::State& world, ref<Patch> patch, Rtid::Set taintedLines) {
        review::Notes notesAccumulated;
        Rtid::Set taintedLinesAccumulated = taintedLines;

        const auto incoming = base::make_shared<Patch>(world.schema);
        incoming->absorb(*patch);
        patch->clear();

        model::complex::Future advancing(world, patch, base::cannonball::SeeChanges::observable, taintedLines);
        ref<Patch> lastCorrection = incoming;
        int wave = 0;

        for (;;) {
            if (wave >= temp_defence_normalization_waves) {
                notesAccumulated.critical.push_back(
                    std::format("normalization: depth limit {} reached", temp_defence_normalization_waves));
                return notesAccumulated;
            }
            ++wave;

            const auto fix = reactions_pass(advancing, lastCorrection, taintedLinesAccumulated);

            if (fix.notes.rejection()) {
                append(notesAccumulated, fix.notes);
                return notesAccumulated;
            }

            patch->absorb(*lastCorrection);
            append(notesAccumulated, fix.notes);

            const auto fixStats = analysis::Patch{*fix.patch};
            const bool anotherWave = not (fixStats.overall.patchlets == 0 and fix.taintedDuringPatch.empty());
            if (not anotherWave)
                break;

            lastCorrection = fix.patch;
            // TODO: extend taintedLinesAccumulated with fix.taintedDuringPatch
        }

        return notesAccumulated;
    }
}


// facade part
namespace fqsm::processing::actions {

    auto update(model::complex::Reality& state, fqsm::ref<Patch> patch, Rtid::Set taintedLines) -> review::Notes {
        const auto notes = normalization::normalization(state, patch, taintedLines);
        if (not notes.rejection())
            integrate(state, *patch);
        return notes;
    }
}
