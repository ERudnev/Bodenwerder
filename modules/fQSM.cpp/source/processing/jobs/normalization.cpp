#include <fQSM/processing/jobs/normalization.h>

#include <format>
#include <set>

#include <fQSM/processing/jobs/integration.h>
#include <fQSM/processing/contexts/review.h>
#include <fQSM/model/complex/future.h>
#include <fQSM/model/intertype/schema.h>
#include <fQSM/features/reaction.h>

// local alias:
namespace fqsm::processing::jobs {
    static constexpr int temp_defence_normalization_waves = 10;

    using Patch = fqsm::model::complex::Patch;
    using PatchRef = fqsm::ref<Patch>;
}

// internal part of normalization
namespace fqsm::processing::jobs::normalization {

    void append(review::Result& dst, review::Result src) {
        dst.critical.insert(dst.critical.end(), src.critical.begin(), src.critical.end());
        dst.warning.insert(dst.warning.end(), src.warning.begin(), src.warning.end());
    }

    struct PassResult {
        fqsm::ref<model::complex::Patch> patch;
        Rtid::Set taintedDuringPatch;
        review::Result result;
    };

    // build one normalization wave
    auto reactions_pass(Reading source, fqsm::cref<Patch> changes, const Rtid::Set& taintedLines) -> PassResult {
        //base::message("creating review context");
        PassResult pass{
            base::make_shared<Patch>(source.schema),
            {}, // TODO: consider filling Tainted Flags once Reviewers will become context::Direct<T> compatible
            {}
        };

        // this is very important place: this cast is saves about ~300 lines of code for new class
        // complex::Proposal === const complex::Draft
        fqsm::ref<Patch> non_const_patch(std::const_pointer_cast<Patch>(changes.std_ptr()));
        const auto proposal = model::complex::Future{source, non_const_patch, taintedLines};
        auto context = Review(
            proposal,
            pass.patch,
            pass.result);

        std::set<model::intertype::Graph::ReactionId> selectedReactions;
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

        return pass;
    }

    review::Result normalization(const model::complex::State& world, ref<Patch> patch, Rtid::Set taintedLines) {
        review::Result accumulated;
        Rtid::Set taintedLinesAccumulated = taintedLines;

        const auto incoming = base::make_shared<Patch>(world.schema);
        incoming->absorb(*patch);
        patch->clear();

        model::complex::Future advancing(world, patch, taintedLines);
        ref<Patch> lastCorrection = incoming;
        int wave = 0;

        for (;;) {
            if (wave >= temp_defence_normalization_waves) {
                accumulated.critical.push_back(
                    std::format("normalization: depth limit {} reached", temp_defence_normalization_waves));
                return accumulated;
            }
            ++wave;

            const auto fix = reactions_pass(advancing, lastCorrection, taintedLinesAccumulated);

            if (not fix.result.good()) {
                append(accumulated, fix.result);
                return accumulated;
            }

            patch->absorb(*lastCorrection);
            append(accumulated, fix.result);

            const bool anotherWave = not (fix.patch->quanta() == 0 and fix.taintedDuringPatch.empty());
            if (not anotherWave)
                break;

            lastCorrection = fix.patch;
            // TODO: extend taintedLinesAccumulated with fix.taintedDuringPatch
        }

        return accumulated;
    }
}


// facade part
namespace fqsm::processing::jobs {

    auto update(model::complex::Reality& state, fqsm::ref<Patch> patch, Rtid::Set taintedLines) -> review::Result {
        const auto result = normalization::normalization(state, patch, taintedLines);
        if (result.good())
            integrate(state, *patch);
        return result;
    }
}
