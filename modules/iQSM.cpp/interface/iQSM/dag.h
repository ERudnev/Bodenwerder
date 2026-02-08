#pragma once

#include <memory>
#include <map>
#include <set>
#include <typeindex>

#include <iQSM/aspects.h>
#include <iQSM/field.h>

namespace iqsm {

    // local forward:
    struct DagState;

    // Handle
    using Dag = cref<DagState>;

    struct DagState {
        using TypeId = internals::Types::RuntimeId;
        using TypeSet = std::set<TypeId>;

        struct Entry {
            TypeSet depends_from;
            TypeSet they_depend;
            UField zero;
        };

        std::map<TypeId, Entry> aspects;

        template<Facet... TypeList>
        static Dag define();

    private:
        void check_closed() const;
        void update_dependents();

        template<Facet Meta>
        static TypeSet depends_of();
    };  
}

template<iqsm::Facet... TypeList>
iqsm::Dag iqsm::DagState::define()
{
    auto result = std::make_shared<DagState>();

    (result->aspects.emplace(Aspect<TypeList>::typeId, Entry{
        depends_of<TypeList>(),
        TypeSet{},
        std::static_pointer_cast<const FieldUntyped>(std::make_shared<const FieldState<TypeList>>())
    }), ...);

    result->check_closed();
    result->update_dependents();
    return result;
}

template<iqsm::Facet Meta>
iqsm::DagState::TypeSet iqsm::DagState::depends_of()
{
    TypeSet out;
    if constexpr (requires { Meta::depends(); }) {
        for (const auto& t : Meta::depends()) { out.insert(t); }
    }
    out.erase(Aspect<Meta>::typeId);
    return out;
}

