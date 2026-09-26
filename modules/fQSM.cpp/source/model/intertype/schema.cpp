#include <fQSM/model/intertype/schema.h>

#include <format>
#include <stdexcept>

#include <fQSM/features/reaction.h>

namespace fqsm::model::intertype {

    void Graph::deriveLinks() {
        links.clear();
        linksOfClient.assign(descriptors.size(), {});
        for (const auto& reaction : reactions) {
            for (const auto& spec : reaction->links()) {
                const auto client = nodes.find(spec.client);
                const auto observed = nodes.find(spec.observed);
                if (client == nodes.end() or observed == nodes.end()) continue;
                if (linkOf(client->second.slot, observed->second.slot, spec.read) != npos) continue;
                linksOfClient[client->second.slot].push_back(links.size());
                links.push_back(erased::Link{client->second.slot, observed->second.slot, spec.read});
            }
        }
    }

    std::size_t Graph::linkOf(Slot client, Slot observed, erased::LinkReader read) const {
        for (std::size_t i = 0; i < links.size(); ++i)
            if (links[i].client == client and links[i].observed == observed and links[i].read == read) return i;
        return npos;
    }

    void Graph::requireHosts() const {
        for (const auto& descriptor : descriptors) {
            if (not descriptor.host or nodes.contains(*descriptor.host)) continue;
            throw std::logic_error(std::format("fQSM schema: aspect {} needs its host {}, which is not in the schema",
                descriptor.name, Rtid::name(*descriptor.host)));
        }
    }

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
