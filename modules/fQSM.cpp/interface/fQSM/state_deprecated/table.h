#pragma once

#include <vector>

#include <base/containers_deprecated/relations.h>
#include <fQSM/meta/interface.include.h>

namespace fqsm::state::relations {

    template<aspect::Any MetaOrigin, aspect::Any MetaFollower>
    class Table {
    public:
        using Origin = Id<MetaOrigin>;
        using Follower = Id<MetaFollower>;

        std::vector<Origin> origins(Follower follower) const;
        std::vector<Follower> followers(Origin origin) const;

        bool insert_relation(Origin origin, Follower follower) { return relations.insert(origin.raw(), follower.raw()); }
        bool erase_relation(Origin origin, Follower follower) { return relations.erase(origin.raw(), follower.raw()); }

        void erase_origin(Origin origin) { relations.erase_a(origin.raw()); }
        void erase_follower(Follower follower) { relations.erase_b(follower.raw()); }

    protected:
        using Container = base::Relations<typename Origin::Raw, typename Follower::Raw>;

        Container relations;
    };
}

// Impl
namespace fqsm::state::relations {
    template<aspect::Any MetaOrigin, aspect::Any MetaFollower>
    auto Table<MetaOrigin, MetaFollower>::origins(Follower follower) const -> std::vector<Origin>
    {
        const auto raw = relations.find_b(follower.raw());
        std::vector<Origin> result;
        result.reserve(raw.size());

        for (const auto value : raw) {
            result.push_back(Origin{ value });
        }

        return result;
    }

    template<aspect::Any MetaOrigin, aspect::Any MetaFollower>
    auto Table<MetaOrigin, MetaFollower>::followers(Origin origin) const -> std::vector<Follower>
    {
        const auto raw = relations.find_a(origin.raw());
        std::vector<Follower> result;
        result.reserve(raw.size());

        for (const auto value : raw) {
            result.push_back(Follower{ value });
        }

        return result;
    }
}
