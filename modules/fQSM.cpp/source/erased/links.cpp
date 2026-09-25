#include <fQSM/erased/links.h>

#include <algorithm>

#include <fQSM/erased/line.h>
#include <fQSM/erased/patch_line.h>

namespace fqsm::erased {

    const std::vector<RawId>* InboundIndex::holders(RawId observed) const {
        const auto found = byObserved.find(observed);
        return found == byObserved.end() ? nullptr : &found->second;
    }

    void InboundIndex::add(RawId observed, RawId client) {
        auto& clients = byObserved[observed];
        if (std::find(clients.begin(), clients.end(), client) == clients.end())
            clients.push_back(client);
    }

    void InboundIndex::remove(RawId observed, RawId client) {
        const auto found = byObserved.find(observed);
        if (found == byObserved.end()) return;
        auto& clients = found->second;
        const auto at = std::find(clients.begin(), clients.end(), client);
        if (at != clients.end()) {
            *at = clients.back();
            clients.pop_back();
        }
        if (clients.empty()) byObserved.erase(found);
    }

    void InboundIndex::clear() {
        byObserved.clear();
    }

    void InboundIndex::rebuild(const Line& clients, LinkReader read) {
        clear();
        const auto end = clients.cursor_end();
        for (auto it = clients.cursor_begin(); not (it == end); ++it) {
            const auto entry = *it;
            if (const RawId observed = read(entry.value))
                add(observed, entry.id);
        }
    }

    void InboundIndex::apply(const Line& clients, const PatchLine& patch, LinkReader read) {
        for (std::size_t i = 0; i < patch.count(); ++i) {
            const RawId client = patch.id_at(i);
            const auto patchlet = patch.at(i);
            const void* current = clients.find(client);
            const RawId before = current ? read(current) : RawId{0};
            const RawId after = patchlet.tombstone ? RawId{0} : read(patchlet.value);
            if (before == after) continue;
            if (before) remove(before, client);
            if (after) add(after, client);
        }
    }
}
