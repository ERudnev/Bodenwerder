#pragma once

#include <format>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include <base/shared_reference.h>

#include <iQSM/_forwards.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/internals/schema_entries.h>
#include <iQSM/internals/type_list.h>
#include <iQSM/meta/aspect_id.h>
#include <iQSM/operations/interface.h>
#include <iQSM/require.h>
#include <iQSM/references.h>
#include <iQSM/types.h>

namespace iqsm {
    struct SchemaObject {
        using TypeId = internals::Types::RuntimeId;
        using TypeSet = std::set<TypeId>;
        using Invariants = detail::validation::Block;

        struct Entry {
            std::string name;
            TypeSet require;
            TypeSet required_by;
            internals::schema::FieldEntry field;
            internals::schema::ResourceEntry resource;
            Invariants invariants;
            internals::schema::DeltaEntry delta;
        };

        std::map<TypeId, Entry> aspects;
        bool empty() const { return aspects.empty(); }

        bool depends(TypeId depender, TypeId dependee) const; // transitively

        template<meta::Aspect... Leaves>
        static SchemaObject assemble();

        static Schema merge(Schema first, Schema second);

    private:
        void check_closed() const;
        void update_required_by();

        template<typename... Ts>
        static TypeSet requirements_of(internals::type_list<Ts...>) {
            return TypeSet{types::aspectId<Ts>()...};
        }

        template<meta::Aspect Meta>
        static TypeSet requirements_of() {
            return requirements_of(typename internals::deps_of<Meta>::type{});
        }

        template<typename List>
        struct assemble_from;
    };
}

