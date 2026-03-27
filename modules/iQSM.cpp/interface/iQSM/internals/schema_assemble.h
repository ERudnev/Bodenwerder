#pragma once

#include <iQSM/schema.h>

#include <iQSM/field.h>
#include <iQSM/internals/delta_builders.h>
#include <iQSM/types.h>

namespace iqsm {
    namespace internals::schema_thunks {
        template<meta::Aspect Meta>
        struct delta_entry {
            using UField = cref<::iqsm::delta::FieldDiffAbstract>;
            using UFieldMut = ref<::iqsm::delta::FieldDiffAbstract>;
            using FD = ::iqsm::delta::FieldDiff<Meta>;

            static auto as(UField d) -> cref<const FD> { return base::shared_ref_cast<const FD>(d); }
            static auto as_mut(UFieldMut d) -> ref<FD> { return base::shared_ref_cast<FD>(d); }

            static auto make_delta_field(cref<FieldAbstract> from, cref<FieldAbstract> to) -> std::optional<UField> {
                return iqsm::internals::delta::count_delta_field<Meta>(std::move(from), std::move(to));
            }

            static auto integrate_field(cref<FieldAbstract> current, UField diff) -> cref<FieldAbstract> { return as(diff)->integrate(std::move(current)); }
            static bool empty(UField diff) { return as(diff)->empty(); }
            static auto clone(UField diff) -> UFieldMut { return as(diff)->clone(); }
            static void absorb(UFieldMut lhs, UField rhs) { as_mut(lhs)->absorb(*as(rhs)); }
        };
    }

    template<meta::Aspect Meta>
    internals::schema::FieldEntry internals::schema::FieldEntry::make() {
        return internals::schema::FieldEntry{base::make_shared<const FieldData<Meta>>()};
    }

    template<meta::Aspect Meta>
    internals::schema::DeltaEntry internals::schema::DeltaEntry::make() {
        return internals::schema::DeltaEntry{
            &internals::schema_thunks::delta_entry<Meta>::make_delta_field,
            &internals::schema_thunks::delta_entry<Meta>::integrate_field,
            &internals::schema_thunks::delta_entry<Meta>::empty,
            &internals::schema_thunks::delta_entry<Meta>::clone,
            &internals::schema_thunks::delta_entry<Meta>::absorb,
        };
    }

    template<meta::Aspect... Leaves>
    SchemaObject SchemaObject::assemble() {
        using closed = typename internals::tl_closure<
            internals::type_list<Leaves...>,
            internals::type_list<>
        >::type;
        return assemble_from<closed>::run();
    }

    template<meta::Aspect... TypeList>
    struct SchemaObject::assemble_from<internals::type_list<TypeList...>> {
        static SchemaObject run() {
            SchemaObject out{};
            (out.aspects.emplace(types::aspectId<TypeList>(), Entry{
                std::string{internals::type_name<TypeList>()},
                requirements_of<TypeList>(),
                TypeSet{},
                internals::schema::FieldEntry::make<TypeList>(),
                TypeList::invariants,
                internals::schema::DeltaEntry::make<TypeList>(),
            }), ...);

            out.check_closed();
            out.update_required_by();
            return out;
        }
    };
}

