#pragma once

// "Layer" is a heterogenous set of slices with one order and one storage mutability.

#include <unordered_map>

#include <fQSM/meta/axis.h>
#include <fQSM/meta/specializations.h>
#include <fQSM/state/_forwards.h>
#include <fQSM/state/slice.h>


namespace fqsm::state::composite {
    namespace axis = meta::axis;
    namespace aspect = meta::aspect;

    template<axis::order Order>
    struct View {
        struct Entry {
            using Abstract = slice::Abstract<Order>;

            template<aspect::Any Meta>
            using Typed = slice::View<Meta, Order>;

            template<aspect::Any Meta>
            using Handle = cref<Typed<Meta>>;
        };

        template<aspect::Any Meta>
        auto slice() const -> typename Entry::template Handle<Meta> {
            using ViewSlice = typename Entry::template Typed<Meta>;
            return base::shared_ref_cast<const ViewSlice>(
                slice(aspect::Rtid::of<Meta>())
            );
        }

    protected:
        virtual auto slice(aspect::Rtid runtimeTypeId) const -> cref<typename Entry::Abstract> = 0;
    };


    template<axis::order Order>
    struct Data : View<Order> {
        struct Entry {
            using Abstract = slice::Abstract<Order>;

            template<aspect::Any Meta>
            using Typed = slice::Data<Meta, Order>;

            template<aspect::Any Meta>
            using Handle = ref<Typed<Meta>>;
        };

        using Slices = std::unordered_map<aspect::Rtid, ref<typename Entry::Abstract>, aspect::Rtid::Hash>;

        template<aspect::Any Meta>
        auto slice() -> typename Entry::template Handle<Meta> {
            using DataSlice = typename Entry::template Typed<Meta>;
            return base::shared_ref_cast<DataSlice>(slices.at(aspect::Rtid::of<Meta>()));
        }

        Slices slices;

    protected:
        auto slice(aspect::Rtid runtimeTypeId) const -> cref<typename View<Order>::Entry::Abstract> override {
            return slices.at(runtimeTypeId);
        }
    };
};