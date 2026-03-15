#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include <iQSM/_forwards.h>
#include <iQSM/delta.h>
#include <iQSM/internals/delta_builders.h>
#include <iQSM/meta.h>
#include <iQSM/field.h>
#include <iQSM/internals/type_list.h>
#include <iQSM/types.h>

namespace iqsm {

    struct SchemaObject {
        using TypeId = internals::Types::RuntimeId;
        using TypeSet = std::set<TypeId>;

        struct Invariants {
            std::vector<ops::validation::Func> own; // TODO: merge with externally attached invariants (union without duplicates)
        };

        struct Entry {
            using MakeDeltaField = std::optional<iqsm::delta::UField>(*)(FieldAbstract::Ref from, FieldAbstract::Ref to);

            std::string name;
            TypeSet require;
            TypeSet required_by;
            FieldAbstract::Ref zero;
            Invariants invariants;
            MakeDeltaField make_delta_field = nullptr;
        };

        std::map<TypeId, Entry> aspects;
        bool empty() const { return aspects.empty(); }
        bool depends(TypeId depender, TypeId dependee) const; //  transitively.

        template<meta::Aspect... Leaves>
        static SchemaObject assemble();
        static Schema merge(Schema first, Schema second);

    private:
        void check_closed() const;
        void update_required_by();

        template<typename List>
        struct assemble_from;

        template<meta::Aspect Meta>
        static TypeSet requirements_of();
    };
}

template<iqsm::meta::Aspect... Leaves>
iqsm::SchemaObject iqsm::SchemaObject::assemble()
{
    using closed = typename internals::tl_closure<
        internals::type_list<Leaves...>,
        internals::type_list<>
    >::type;
    return assemble_from<closed>::run();
}

template<iqsm::meta::Aspect... TypeList>
struct iqsm::SchemaObject::assemble_from<iqsm::internals::type_list<TypeList...>> {
    static SchemaObject run() {
        SchemaObject out{};
        (out.aspects.emplace(Facet<TypeList>::typeId, Entry{
            std::string{Facet<TypeList>::typeName},
            requirements_of<TypeList>(),
            TypeSet{},
            base::make_shared<const FieldObject<TypeList>>(),
            Invariants{ .own = TypeList::invariants.list },
            &iqsm::internals::delta::count_delta_field<TypeList>
        }), ...);

        out.check_closed();
        out.update_required_by();
        return out;
    }
};

template<iqsm::meta::Aspect Meta>
iqsm::SchemaObject::TypeSet iqsm::SchemaObject::requirements_of()
{
    TypeSet out;
    if constexpr (requires { Meta::depends(); }) {
        for (const auto& t : Meta::depends()) { out.insert(t); }
    }
    out.erase(Facet<Meta>::typeId);
    return out;
}