#include <fQSM/processing/algorithms/structural.h>

#include <unordered_set>
#include <utility>
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

        // Removed ids by slot: what one closure pass reads, and the deletions it writes for the next pass.
        using Removed = std::vector<std::vector<RawId>>;

        template<typename Visit>
        void for_each_change(const Future& proposal, Slot slot, DeltaLayer layer, Visit&& visit) {
            const auto end = proposal.delta_end(slot, layer);
            for (auto it = proposal.delta_begin(slot, layer); not (it == end); ++it)
                visit(*it);
        }

        void refuse(Future& refusals, std::string message) {
            refusals.summary().critical.push_back(std::move(message));
        }

        bool checks_additions(RuleKind kind) {
            return kind == RuleKind::newRequiresExistingParent
                or kind == RuleKind::newRequiresParentAppears
                or kind == RuleKind::parentAppearsRequiresComponent;
        }

        // The refusal rules read the additions of the proposal as the writer left it, before the closure deletes anything.
        void check_additions(const erased::Rule& rule, const Future& proposal, Future& refusals) {
            const auto& descriptors = proposal.schema->descriptors;
            const auto name = [&](Slot slot) { return descriptors[slot].name; };

            switch (rule.kind) {
            case RuleKind::newRequiresExistingParent:
                for_each_change(proposal, rule.listens, DeltaLayer::added, [&](const erased::Change& change) {
                    if (proposal.line(rule.other).find(change.id)) return;
                    refuse(refusals, utility::messages::structural_missing(name(rule.other), name(rule.subject), change.id));
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
                    refuse(refusals, utility::messages::structural_same_patch(name(rule.other), name(rule.subject), change.id));
                });
                break;
            }

            case RuleKind::parentAppearsRequiresComponent:
                for_each_change(proposal, rule.listens, DeltaLayer::added, [&](const erased::Change& change) {
                    if (proposal.line(rule.subject).find(change.id)) return;
                    refuse(refusals, utility::messages::structural_missing(name(rule.subject), name(rule.other), change.id));
                });
                break;

            default:
                break;
            }
        }

        // A deletion the rules write into the proposal. Only an id that is still visible counts as a new removal,
        // so a cascade that reaches an id twice, or an id the writer already deleted, produces nothing.
        void remove(Future& proposal, Slot slot, RawId id, Removed& produced) {
            if (not proposal.line(slot).find(id)) return;
            proposal.writer(slot).put_deletion(id);
            produced[slot].push_back(id);
        }

        void apply_removals(const erased::Rule& rule, const std::vector<RawId>& removed, Future& proposal, Removed& produced) {
            const auto& descriptors = proposal.schema->descriptors;

            switch (rule.kind) {
            case RuleKind::removeWithParent:
                for (const RawId id : removed)
                    remove(proposal, rule.subject, id, produced);
                break;

            case RuleKind::deadParasiticKillsParent:
                for (const RawId id : removed)
                    remove(proposal, rule.other, id, produced);
                break;

            case RuleKind::groupRemovalRemovesElements: {
                // the tombstone of the group keeps its last value: the element ids
                const auto elements = descriptors[rule.subject].groupElements;
                const auto* line = proposal.patch()->line(rule.listens);
                if (not line) break;
                std::vector<RawId> ids;
                for (const RawId group : removed) {
                    const auto last = line->mention(group);
                    if (not last.found) continue;
                    ids.clear();
                    elements(last.value, ids);
                    for (const RawId id : ids)
                        remove(proposal, rule.other, id, produced);
                }
                break;
            }

            case RuleKind::elementRemovalUnhooks: {
                // a group modification feeds no removal rule, so it is not a produced removal
                const auto& group = descriptors[rule.subject];
                const auto& members = proposal.line(rule.subject);
                auto& target = proposal.writer(rule.subject);
                erased::Slots scratch(*group.quantum);
                std::vector<std::pair<RawId, erased::Slots::Index>> hits;   // id, slot in scratch: written after the scan
                const auto end = members.cursor_end();
                for (auto it = members.cursor_begin(); not (it == end); ++it) {
                    const auto entry = *it;
                    const auto index = static_cast<erased::Slots::Index>(scratch.push_copy(entry.value));
                    bool hit = false;
                    for (const RawId id : removed)
                        if (group.groupErase(scratch.at(index), id)) hit = true;
                    if (hit) hits.emplace_back(entry.id, index);
                }
                for (const auto [id, index] : hits)
                    target.put_modification(id, scratch.at(index));
                break;
            }

            default:
                break;
            }
        }
    }

    void apply_structural_rules(Future& proposal, Future& refusals, const meta::Rtid::Set& tainted) {
        const auto& schema = *proposal.schema;
        const auto patch = proposal.patch();
        const auto active = [&](Slot slot) {
            const auto* line = patch->line(slot);
            return (line and line->has_changes()) or tainted.contains(schema.descriptors[slot].id);
        };

        for (const auto& rule : schema.rules)
            if (checks_additions(rule.kind) and active(rule.listens))
                check_additions(rule, proposal, refusals);

        // the removals of the proposal, collected before any rule writes into it; most transactions have none,
        // so nothing is allocated before the first removal is seen
        std::vector<std::pair<Slot, RawId>> initial;
        {
            Slot scanned[32];   // slots scanned once; past 32 a slot may be scanned again, which only repeats an id
            std::size_t scannedCount = 0;
            for (const auto& rule : schema.rules) {
                if (checks_additions(rule.kind) or not active(rule.listens)) continue;
                bool seen = false;
                for (std::size_t i = 0; i < scannedCount; ++i)
                    if (scanned[i] == rule.listens) { seen = true; break; }
                if (seen) continue;
                if (scannedCount < 32) scanned[scannedCount++] = rule.listens;
                for_each_change(proposal, rule.listens, DeltaLayer::removed, [&](const erased::Change& change) {
                    initial.emplace_back(rule.listens, change.id);
                });
            }
        }
        if (initial.empty()) return;

        Removed removed(schema.slotCount());
        for (const auto [slot, id] : initial)
            removed[slot].push_back(id);

        // closure: the deletions of one pass are the removals of the next, until a pass deletes nothing
        for (;;) {
            Removed produced(schema.slotCount());
            for (const auto& rule : schema.rules) {
                if (checks_additions(rule.kind)) continue;
                const auto& ids = removed[rule.listens];
                if (not ids.empty()) apply_removals(rule, ids, proposal, produced);
            }
            bool any = false;
            for (const auto& ids : produced)
                if (not ids.empty()) { any = true; break; }
            if (not any) break;
            removed = std::move(produced);
        }
    }
}
