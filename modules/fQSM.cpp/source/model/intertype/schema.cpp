#include <fQSM/model/intertype/schema.h>

namespace fqsm::model::intertype {

    void Graph::deriveRules() {
        using erased::Category;
        using erased::RuleKind;

        rules.clear();
        for (Slot slot = 0; slot < descriptors.size(); ++slot) {
            const auto& descriptor = descriptors[slot];
            if (descriptor.category == Category::entity or not descriptor.host) continue;

            const auto host = nodes.find(*descriptor.host);
            if (host == nodes.end()) continue;
            const Slot parent = host->second.slot;
            const auto add = [&](RuleKind kind, Slot listens, Slot other) {
                rules.push_back(erased::Rule{kind, listens, slot, other});
            };

            add(RuleKind::removeWithParent, parent, parent);
            switch (descriptor.category) {
            case Category::attribute:
                add(RuleKind::newRequiresExistingParent, slot, parent);
                break;
            case Category::feature:
                add(RuleKind::deadParasiticKillsParent, slot, parent);
                add(RuleKind::newRequiresParentAppears, slot, parent);
                break;
            case Category::component:
                add(RuleKind::deadParasiticKillsParent, slot, parent);
                add(RuleKind::parentAppearsRequiresComponent, parent, parent);
                add(RuleKind::newRequiresParentAppears, slot, parent);
                break;
            case Category::group: {
                add(RuleKind::deadParasiticKillsParent, slot, parent);
                if (const auto element = nodes.find(*descriptor.element); element != nodes.end()) {
                    add(RuleKind::groupRemovalRemovesElements, slot, element->second.slot);
                    add(RuleKind::elementRemovalUnhooks, element->second.slot, element->second.slot);
                }
                add(RuleKind::parentAppearsRequiresComponent, parent, parent);
                add(RuleKind::newRequiresParentAppears, slot, parent);
                break;
            }
            case Category::entity:
                break;
            }
        }
    }
}
