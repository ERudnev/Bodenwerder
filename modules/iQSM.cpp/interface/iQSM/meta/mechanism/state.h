#pragma once

#include <unordered_map>
#include <optional>

#include <base/maybe.h>
#include <iQSM/typeId.h>
#include <iQSM/meta/axis.h>
#include <iQSM/meta/alias.h>
#include <iQSM/meta/concepts/archetype.h>
#include <iQSM/state/_forwards.h>


namespace iqsm::meta::state {
    template<archetype::Any, axis::versioning, axis::order>
    struct ItemsLayout;

    template<axis::versioning SliceVersioning>
    struct SlicesLayout;

    template<archetype::Any, axis::versioning ItemVersioning>
    struct Differentiation;
}


namespace iqsm::meta::state {

    namespace patch {
        // TODO: find better alias as template<versioning>...

        //template<typename Meta>
        //using FlatPatch = std::optional<Quantum<Meta>>;
        template<archetype::Any Meta>
        //struct Solid : base::maybe<Quantum<Meta>> {
        using Solid = base::maybe<Quantum<Meta>>;

        template<archetype::Any Meta>
        struct Shared {
            std::optional<Node<Meta>> before;
            std::optional<Node<Meta>> after;

            bool is_noop() const { return !before && !after; }
            bool is_add()  const { return !before &&  after; }
            bool is_del()  const { return  before && !after; }
            bool is_chg()  const { return  before &&  after; }
        };
    }

    // Items Layout specs:
    template<archetype::Any Meta>
    struct ItemsLayout<Meta, axis::versioning::shared, axis::order::state> {
        using Element = Node<Meta>;
    };

    template<archetype::Any Meta>
    struct ItemsLayout<Meta, axis::versioning::shared, axis::order::patch> {
        using Element = patch::Shared<Meta>;
    };

    template<archetype::Any Meta>
    struct ItemsLayout<Meta, axis::versioning::single, axis::order::state> {
        using Element = Quantum<Meta>;
    };

    template<archetype::Any Meta>
    struct ItemsLayout<Meta, axis::versioning::single, axis::order::patch> {
        using Element = patch::Solid<Meta>;
    };


    // Slices Layout specs:
    template<>
    struct SlicesLayout<axis::versioning::shared> {  
        template<typename SliceBase>
        using RefQualified = iqsm::cref<SliceBase>;

        template<typename SliceBase>
        using Container = std::unordered_map<RAId, RefQualified<SliceBase>>; // TODO: std::map -> base::DenseTable

        template<typename SliceBase>
        struct ZeroSliceProvider {

            template<typename ActualSliceType>
            static ZeroSliceProvider create() {
                return ZeroSliceProvider{ .prototype = prototypeTypedSingleton<ActualSliceType>() };
            }

            iqsm::cref<SliceBase> provide() const {
                return prototype;
            }

            const iqsm::cref<SliceBase> prototype;
        private:
            template<typename ActualSliceType>
            static iqsm::cref<SliceBase> prototypeTypedSingleton() {
                static iqsm::cref<SliceBase> prototype = iqsm::freeze(base::make_shared<ActualSliceType>());
                return prototype;
            }
            
        };
    };

    template<>
    struct SlicesLayout<axis::versioning::single> {
        template<typename SliceBase>
        using RefQualified = iqsm::ref<SliceBase>;

        template<typename SliceBase>
        using Container = std::unordered_map<RAId, RefQualified<SliceBase>>; // TODO: std::map -> base::DenseTable

        template<typename SliceBase>
        struct ZeroSliceProvider {
            template<typename ActualSliceType>
            static ZeroSliceProvider create() {
                return ZeroSliceProvider(iqsm::freeze(base::make_shared<ActualSliceType>()));
            }

            iqsm::ref<SliceBase> provide() const {
                return iqsm::clone(prototype);
            }

        private:
            ZeroSliceProvider(iqsm::cref<SliceBase> prototype) : prototype(prototype) {}

            const iqsm::cref<SliceBase> prototype;
        };
    };


    // Differentiation (converting state <-> patch):
    template<archetype::Any Meta>
    struct Differentiation<Meta, axis::versioning::shared> {
        using State = typename Meta::Runtime::Element::State;
        using Patch = typename Meta::Runtime::Element::Patch;

        static Patch add(State after) { return Patch{ std::nullopt, std::move(after) }; }
        static Patch remove(State before) { return Patch{ std::move(before), std::nullopt };}
        static Patch change(State before, State after) { return Patch{ std::move(before), std::move(after) };}
    };

    template<archetype::Any Meta>
    struct Differentiation<Meta, axis::versioning::single> {
        using State = typename Meta::Runtime::Element::State;
        using Patch = typename Meta::Runtime::Element::Patch;

        static Patch add(State after) { return Patch{std::move(after)}; }
        static Patch remove(State before) { return Patch{std::nullopt}; }
        static Patch change(State before, State after) { return Patch{std::move(after)}; }
    };
}


