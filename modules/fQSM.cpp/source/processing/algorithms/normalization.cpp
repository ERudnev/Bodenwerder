#include <fQSM/processing/algorithms/normalization.h>

#include <format>
#include <optional>
#include <set>

#include <fQSM/processing/algorithms/integration.h>
#include <fQSM/processing/algorithms/structural.h>
#include <fQSM/model/complex/future.h>
#include <fQSM/model/intertype/schema.h>
#include <fQSM/features/reaction.h>
#include <fQSM/utility/logging.h>

#ifdef FQSM_WAVE_STATS
#include <chrono>
#include <cstdint>
#include <cstdio>

// Opt-in probe (CMake option FQSM_WAVE_STATS): one stderr line per 5 s window. Single-threaded use.
namespace fqsm::processing::algorithm::probe {
    using Clock = std::chrono::steady_clock;

    struct Window {
        std::uint64_t transactions = 0;
        std::uint64_t waves = 0;
        std::uint64_t reactions = 0;
        std::uint64_t micros = 0;
        std::uint64_t byWaves[6] = {};   // 0, 1, 2, 3, 4, 5+ waves
        int maxWaves = 0;
        Clock::time_point opened = Clock::now();
    };

    inline Window window;

    struct Transaction {
        Clock::time_point started = Clock::now();
        int waves = 0;

        ~Transaction() {
            const auto now = Clock::now();
            window.transactions += 1;
            window.waves += static_cast<std::uint64_t>(waves);
            window.byWaves[waves < 5 ? waves : 5] += 1;
            if (waves > window.maxWaves) window.maxWaves = waves;
            window.micros += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now - started).count());
            if (now - window.opened < std::chrono::seconds(5)) return;
            std::fprintf(stderr, "fQSM waves: tx=%llu waves=%llu [1:%llu 2:%llu 3:%llu 4:%llu 5+:%llu max:%d] reactions=%llu normalization=%llu us avg=%.1f us\n",
                static_cast<unsigned long long>(window.transactions), static_cast<unsigned long long>(window.waves),
                static_cast<unsigned long long>(window.byWaves[1]), static_cast<unsigned long long>(window.byWaves[2]),
                static_cast<unsigned long long>(window.byWaves[3]), static_cast<unsigned long long>(window.byWaves[4]),
                static_cast<unsigned long long>(window.byWaves[5]), window.maxWaves,
                static_cast<unsigned long long>(window.reactions), static_cast<unsigned long long>(window.micros),
                static_cast<double>(window.micros) / static_cast<double>(window.transactions));
            window = Window{};
        }
    };
}
#define FQSM_PROBE(...) __VA_ARGS__
#else
#define FQSM_PROBE(...)
#endif

namespace fqsm::processing::algorithm {
    static constexpr int temp_defence_normalization_waves = 10;
    using Patch = fqsm::model::complex::Patch;
}

// internal part of normalization
namespace fqsm::processing::algorithm::normalization {

    void append(model::complex::Patch::Summary& dst, const model::complex::Patch::Summary& src) {
        dst.critical.insert(dst.critical.end(), src.critical.begin(), src.critical.end());
        dst.warning.insert(dst.warning.end(), src.warning.begin(), src.warning.end());
    }

    // build one normalization wave: structural rules and reactions read the proposal and write a new pass patch
    auto reactions_pass(const model::complex::State& source, const model::complex::State& origin, fqsm::cref<Patch> changes, const Rtid::Set& taintedLines) -> ref<Patch> {
        auto pass = base::make_shared<Patch>(source);

        // this is very important place: this cast is saves about ~300 lines of code for new class
        // the proposal is a Future over a const patch
        fqsm::ref<Patch> non_const_patch(std::const_pointer_cast<Patch>(changes.std_ptr()));
        auto proposal = model::complex::Future{source, non_const_patch, taintedLines};
        Session corrections(proposal, pass);
        std::optional<Session> retrospective;
        const Reacting context(proposal, corrections, origin, retrospective);

        // the structural closure writes into the proposal: the reactions below see the whole cascade
        apply_structural_rules(proposal, corrections.view, taintedLines);

        const auto& schema = *changes->schema;
        std::set<model::intertype::Graph::ReactionId> selectedReactions;
        const auto select = [&](meta::Rtid sourceType) {
            const auto found = schema.nodes.find(sourceType);
            if (found == schema.nodes.end()) return;
            selectedReactions.insert(found->second.reactions.begin(), found->second.reactions.end());
        };
        for (Patch::Slot slot = 0; slot < schema.slotCount(); ++slot) {
            const auto* line = changes->line(slot);
            if (line and line->has_changes())
                select(schema.descriptors[slot].id);
        }
        for (const auto& sourceType : taintedLines)
            select(sourceType);

        for (const auto reactionId : selectedReactions) {
            changes->schema->reactions.at(reactionId.raw())->apply(context);
        }
        FQSM_PROBE(probe::window.reactions += selectedReactions.size();)

        _DBG_TX_("norm pass: {} reactions, changes={}, reaction={}", selectedReactions.size(), utility::format_patch(changes), utility::format_patch(fqsm::freeze(pass)));
        return pass;
    }

    model::complex::Patch::Summary normalization(const model::complex::State& world, ref<Patch> patch, Rtid::Set taintedLines) {
        model::complex::Patch::Summary accumulated;
        FQSM_PROBE(probe::Transaction measured;)

        _DBG_TX_("norm: start user patch={}", utility::format_patch(fqsm::freeze(patch)));

        append(accumulated, patch->summary);
        if (not patch->summary.good()) {
            return accumulated;
        }

        // accepted: what the waves have accepted so far, seen through advancing. The user's patch is the correction
        // under review of wave 1. An accepted correction moves into accepted (values moved, not copied), and at the
        // end the lines of accepted become the lines of the patch: no patchlet is copied on the way to integration.
        const auto accepted = base::make_shared<Patch>(world);
        {
            // Incoming taint fuels wave 1 only. The original set is left as the caller passed it; later waves get none.
            model::complex::Future advancing(world, accepted, taintedLines);
            ref<Patch> lastCorrection = patch;
            int wave = 0;
            auto waveTaint = taintedLines;

            for (;;) {
                if (wave >= temp_defence_normalization_waves) {
                    _DBG_TX_("norm: DEPTH LIMIT {}", temp_defence_normalization_waves);
                    accumulated.critical.push_back(
                        std::format("normalization: depth limit {} reached", temp_defence_normalization_waves));
                    accumulated.waves = wave;
                    return accumulated;
                }
                ++wave;
                FQSM_PROBE(measured.waves = wave;)

                _DBG_TX_("norm: wave {} correction={}", wave, utility::format_patch(fqsm::freeze(lastCorrection)));

                const auto fix = reactions_pass(advancing, world, lastCorrection, waveTaint);
                append(accumulated, fix->summary);
                accumulated.waves = wave;

                if (not fix->summary.good()) {
                    _DBG_TX_("norm: wave {} REJECT critical={} warning={}", wave, fix->summary.critical.size(), fix->summary.warning.size());
                    return accumulated;
                }

                accepted->absorb_move(*lastCorrection);

                const bool anotherWave = fix->has_changes();
                _DBG_TX_("norm: wave {} merged={}, reaction={}, another={}", wave, utility::format_patch(fqsm::freeze(accepted)), utility::format_patch(fqsm::freeze(fix)), anotherWave);
                if (not anotherWave)
                    break;

                lastCorrection = fix;
                waveTaint.clear();
            }
        }
        // advancing is gone: its future lines no longer point at the lines of accepted
        patch->swap_lines(*accepted);

        _DBG_TX_("norm: done final patch={}", utility::format_patch(fqsm::freeze(patch)));
        return accumulated;
    }
}


// facade part
namespace fqsm::processing::algorithm {

    auto update(model::complex::Reality& state, fqsm::ref<Patch> patch, Rtid::Set taintedLines) -> model::complex::Patch::Summary {
        const auto result = normalization::normalization(state, patch, taintedLines);
        if (result.good()) {
            _DBG_TX_("update: INTEGRATE patch={}", utility::format_patch(fqsm::freeze(patch)));
            integrate(state, *patch);
        } else {
            _DBG_TX_("update: REJECT critical={} warning={}", result.critical.size(), result.warning.size());
        }
        return result;
    }
}
