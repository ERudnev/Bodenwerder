#include <fQSM/processing/algorithms/structural.h>

#include <unordered_set>
#include <vector>

#include <fQSM/erased/delta.h>
#include <fQSM/erased/slots.h>
#include <fQSM/model/complex/future.h>
#include <fQSM/utility/messages.h>

namespace fqsm::processing::algorithm {

    namespace {
        using Future = model::complex::Future;
        using Slot = model::complex::State::Slot;
        using erased::DeltaLayer;
        using erased::RuleKind;

        template<typename Visit>
        void for_each_change(const Future& proposal, Slot slot, DeltaLayer layer, Visit&& visit) {
            const auto end = proposal.delta_end(slot, layer);
            for (auto it = proposal.delta_begin(slot, layer); not (it == end); ++it)
                visit(*it);
        }

        void refuse(Future& adjustments, std::string message) {
            adjustments.summary().critical.push_back(std::move(message));
        }

        void apply(const erased::Rule& rule, const Future& proposal, Future& adjustments) {
            const auto& descriptors = proposal.schema->descriptors;
            const auto name = [&](Slot slot) { return descriptors[slot].name; };

            switch (rule.kind) {
            case RuleKind::removeWithParent:
                for_each_change(proposal, rule.listens, DeltaLayer::removed, [&](const erased::Change& change) {
                    adjustments.writer(rule.subject).put_deletion(change.id);
                });
                break;

            case RuleKind::deadParasiticKillsParent:
                for_each_change(proposal, rule.listens, DeltaLayer::removed, [&](const erased::Change& change) {
                    adjustments.writer(rule.other).put_deletion(change.id);
                });
                break;

            case RuleKind::newRequiresExistingParent:
                for_each_change(proposal, rule.listens, DeltaLayer::added, [&](const erased::Change& change) {
                    if (proposal.line(rule.other).find(change.id)) return;
                    refuse(adjustments, utility::messages::structural_missing(name(rule.other), name(rule.subject), change.id));
                });
                break;

            case RuleKind::newRequiresParentAppears: {
                std::unordered_set<RawId> appeared;
                bool collected = false;
                for_each_change(proposal, rule.listens, DeltaLayer::added, [&](const erased::Change& change) {
                    if (not collected) {
                        for_each_change(proposal, rule.other, DeltaLayer::added, [&](const erased::Change& parent) {
                            appeared.insert(parent.id);
                        });
                        collected = true;
                    }
                    if (appeared.contains(change.id)) return;
                    refuse(adjustments, utility::messages::structural_same_patch(name(rule.other), name(rule.subject), change.id));
                });
                break;
            }

            case RuleKind::parentAppearsRequiresComponent:
                for_each_change(proposal, rule.listens, DeltaLayer::added, [&](const erased::Change& change) {
                    if (proposal.line(rule.subject).find(change.id)) return;
                    refuse(adjustments, utility::messages::structural_missing(name(rule.subject), name(rule.other), change.id));
                });
                break;

            case RuleKind::groupRemovalRemovesElements: {
                const auto elements = descriptors[rule.subject].groupElements;
                std::vector<RawId> ids;
                for_each_change(proposal, rule.listens, DeltaLayer::removed, [&](const erased::Change& change) {
                    ids.clear();
                    elements(change.before, ids);
                    for (const RawId id : ids)
                        adjustments.writer(rule.other).put_deletion(id);
                });
                break;
            }

            case RuleKind::elementRemovalUnhooks: {
                std::vector<RawId> removed;
                for_each_change(proposal, rule.listens, DeltaLayer::removed, [&](const erased::Change& change) {
                    removed.push_back(change.id);
                });
                if (removed.empty()) break;

                const auto& group = descriptors[rule.subject];
                const auto& members = proposal.line(rule.subject);
                auto& target = adjustments.writer(rule.subject);
                erased::Slots scratch(*group.quantum);
                const auto end = members.cursor_end();
                for (auto it = members.cursor_begin(); not (it == end); ++it) {
                    const auto entry = *it;
                    scratch.clear();
                    void* next = scratch.at(scratch.push_copy(entry.value));
                    bool hit = false;
                    for (const RawId id : removed)
                        if (group.groupErase(next, id)) hit = true;
                    if (hit) target.put_modification(entry.id, next);
                }
                break;
            }
            }
        }
    }

    void apply_structural_rules(const Future& proposal, Future& adjustments, const meta::Rtid::Set& tainted) {
        const auto& schema = *proposal.schema;
        const auto patch = proposal.patch();
        for (const auto& rule : schema.rules) {
            const auto* line = patch->line(rule.listens);
            const bool active = (line and line->has_changes()) or tainted.contains(schema.descriptors[rule.listens].id);
            if (active) apply(rule, proposal, adjustments);
        }
    }
}
