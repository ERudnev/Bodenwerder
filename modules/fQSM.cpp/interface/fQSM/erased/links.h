#pragma once

// Links between aspects, declared by reactions (custody, anchored), and the inbound index a Reality keeps for each:
// observed id -> the client ids whose link field points at it. Integration keeps the index current; a direct pass
// over the clients (taint) has it rebuilt at the end of the transaction.

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <fQSM/identifier.h>
#include <fQSM/meta/rtid.h>

namespace fqsm::erased {

    class Line;
    class PatchLine;

    // Reads the link field of a client quantum: the raw id of the observed, 0 when none (an unset optional link).
    using LinkReader = RawId (*)(const void* quantum);

    // As a reaction declares it.
    struct LinkSpec {
        meta::Rtid client;
        meta::Rtid observed;
        LinkReader read;
    };

    // As the schema keeps it: by slot. Two declarations with the same client, observed and reader are one link.
    struct Link {
        std::uint32_t client;
        std::uint32_t observed;
        LinkReader read;
    };

    class InboundIndex {
    public:
        // nullptr when nothing links to observed
        const std::vector<RawId>* holders(RawId observed) const;

        void add(RawId observed, RawId client);
        void remove(RawId observed, RawId client);
        void clear();

        // From every client of the line.
        void rebuild(const Line& clients, LinkReader read);
        // Before patch is integrated into clients: forget the old link of every changed id, learn the new one.
        void apply(const Line& clients, const PatchLine& patch, LinkReader read);

    private:
        std::unordered_map<RawId, std::vector<RawId>> byObserved;
    };
}
