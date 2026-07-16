#include <fQSM/processing/algorithms/normalization.h>

#include <format>
#include <set>

#include <fQSM/processing/_forwards.h>
#include <fQSM/processing/algorithms/integration.h>
#include <fQSM/processing/contexts/review.h>
#include <fQSM/model/complex/future.h>
#include <fQSM/model/intertype/schema.h>
#include <fQSM/features/reaction.h>
#include <fQSM/utility/logging.h>

// local alias:
namespace fqsm::processing::algorithm {
    static constexpr int temp_defence_normalization_waves = 10;

    using Patch = fqsm::model::complex::Patch;
    using PatchRef = fqsm::ref<Patch>;
}

// internal part of normalization
namespace fqsm::processing::algorithm::normalization {

    void append(model::complex::Patch::Result& dst, const model::complex::Patch::Result& src) {
        dst.critical.insert(dst.critical.end(), src.critical.begin(), src.critical.end());
        dst.warning.insert(dst.warning.end(), src.warning.begin(), src.warning.end());
    }

    struct PassResult {
        fqsm::ref<model::complex::Patch> patch;
        Rtid::Set taintedDuringPatch;
    };

    // build one normalization wave
    auto reactions_pass(const model::complex::State& source, const model::complex::State& origin, fqsm::cref<Patch> changes, const Rtid::Set& taintedLines) -> PassResult {
        //base::message("creating review context");
        PassResult pass{
            base::make_shared<Patch>(source.schema),
            {}, // TODO: consider filling Tainted Flags once Reviewers will become context::Direct<T> compatible
        };

        // this is very important place: this cast is saves about ~300 lines of code for new class
        // complex::Proposal === const complex::Draft
        fqsm::ref<Patch> non_const_patch(std::const_pointer_cast<Patch>(changes.std_ptr()));
        const auto proposal = model::complex::Future{source, non_const_patch, taintedLines};
        auto context = Review(
            proposal,
            origin,
            pass.patch);

        std::set<model::intertype::Graph::ReactionId> selectedReactions;
        for (const auto& [sourceType, line] : changes->lines.container) {
            if (not line->has_changes() and not taintedLines.contains(sourceType)) {
                continue;
            }
            const auto found = changes->schema->nodes.find(sourceType);
            if (found == changes->schema->nodes.end()) continue;

            for (const auto reactionId : found->second.reactions) {
                selectedReactions.insert(reactionId);
            }
        }

        for (const auto reactionId : selectedReactions) {
            changes->schema->reactions.at(reactionId.raw())->apply(context);
        }

        _DBG_TX_("norm pass: {} reactions, changes={}, reaction={}", selectedReactions.size(), utility::format_patch(changes), utility::format_patch(fqsm::freeze(pass.patch)));
        return pass;
    }

    model::complex::Patch::Result normalization(const model::complex::State& world, ref<Patch> patch, Rtid::Set taintedLines) {
        model::complex::Patch::Result accumulated;
        Rtid::Set taintedLinesAccumulated = taintedLines;

        _DBG_TX_("norm: start user patch={}", utility::format_patch(fqsm::freeze(patch)));

        const auto incoming = base::make_shared<Patch>(world.schema);
        incoming->absorb(*patch);
        patch->clear();
        append(accumulated, incoming->summary);
        if (not incoming->summary.good()) {
            return accumulated;
        }

        _DBG_TX_("norm: incoming={}", utility::format_patch(fqsm::freeze(incoming)));

        model::complex::Future advancing(world, patch, taintedLines);
        ref<Patch> lastCorrection = incoming;
        int wave = 0;

        for (;;) {
            if (wave >= temp_defence_normalization_waves) {
                _DBG_TX_("norm: DEPTH LIMIT {}", temp_defence_normalization_waves);
                accumulated.critical.push_back(
                    std::format("normalization: depth limit {} reached", temp_defence_normalization_waves));
                return accumulated;
            }
            ++wave;

            _DBG_TX_("norm: wave {} correction={}", wave, utility::format_patch(fqsm::freeze(lastCorrection)));

            const auto fix = reactions_pass(advancing, world, lastCorrection, taintedLinesAccumulated);
            append(accumulated, fix.patch->summary);

            if (not fix.patch->summary.good()) {
                _DBG_TX_("norm: wave {} REJECT critical={} warning={}", wave, fix.patch->summary.critical.size(), fix.patch->summary.warning.size());
                return accumulated;
            }

            patch->absorb(*lastCorrection);

            const bool anotherWave = fix.patch->has_changes() or not fix.taintedDuringPatch.empty();
            _DBG_TX_("norm: wave {} merged={}, reaction={}, another={}", wave, utility::format_patch(fqsm::freeze(patch)), utility::format_patch(fqsm::freeze(fix.patch)), anotherWave);
            if (not anotherWave)
                break;

            lastCorrection = fix.patch;
            // TODO: extend taintedLinesAccumulated with fix.taintedDuringPatch
        }

        _DBG_TX_("norm: done final patch={}", utility::format_patch(fqsm::freeze(patch)));
        return accumulated;
    }
}


// facade part
namespace fqsm::processing::algorithm {

    auto update(model::complex::Reality& state, fqsm::ref<Patch> patch, Rtid::Set taintedLines) -> model::complex::Patch::Result {
        const auto result = normalization::normalization(state, patch, taintedLines);
        if (result.good()) {
            _DBG_TX_("update: INTEGRATE patch={}", utility::format_patch(fqsm::freeze(patch)));
            integrate(state, *patch);
        } else {
            _DBG_TX_("update: REJECT critical={} warning={}", result.critical.size(), result.warning.size());
            utility::log_rejected_transaction(result);
        }
        return result;
    }
}
