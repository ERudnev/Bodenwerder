#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include <iQSM/_forwards.h>
#include <iQSM/aspects.h>
#include <iQSM/field.h>
#include <iQSM/internals/type_list.h>
#include <iQSM/types.h>

namespace iqsm {

    struct SchemaObject {
        using TypeId = internals::Types::RuntimeId;
        using TypeSet = std::set<TypeId>;

        struct Invariants {
            std::vector<ops::validation::Func> structural;
            std::vector<ops::validation::Func> logic;
        };

        struct Entry {
            std::string name;
            TypeSet require;
            TypeSet required_by;
            FieldAbstract::Ref zero;
            Invariants invariants;
        };

        std::map<TypeId, Entry> aspects;

        template<Facet... Leaves>
        static SchemaObject assemble();
        static Schema merge(Schema first, Schema second);

    private:
        void check_closed() const;
        void update_required_by();

        template<typename List>
        struct assemble_from;

        template<Facet Meta>
        static TypeSet requirements_of();
    };
}

template<iqsm::Facet... Leaves>
iqsm::SchemaObject iqsm::SchemaObject::assemble()
{
    using closed = typename internals::tl_closure<
        internals::type_list<Leaves...>,
        internals::type_list<>
    >::type;
    return assemble_from<closed>::run();
}

template<iqsm::Facet... TypeList>
struct iqsm::SchemaObject::assemble_from<iqsm::internals::type_list<TypeList...>> {
    static SchemaObject run() {
        SchemaObject out{};
        (out.aspects.emplace(Aspect<TypeList>::typeId, Entry{
            std::string{Aspect<TypeList>::typeName},
            requirements_of<TypeList>(),
            TypeSet{},
            std::static_pointer_cast<const FieldAbstract>(std::make_shared<const FieldObject<TypeList>>()),
            Invariants{ .structural = TypeList::invariants.list }
        }), ...);

        out.check_closed();
        out.update_required_by();
        return out;
    }
};

template<iqsm::Facet Meta>
iqsm::SchemaObject::TypeSet iqsm::SchemaObject::requirements_of()
{
    TypeSet out;
    if constexpr (requires { Meta::depends(); }) {
        for (const auto& t : Meta::depends()) { out.insert(t); }
    }
    out.erase(Aspect<Meta>::typeId);
    return out;
}