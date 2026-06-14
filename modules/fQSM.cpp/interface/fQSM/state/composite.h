#pragma once

#include <unordered_map>

#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/references.h>
#include <fQSM/schema/dag.h>
#include <fQSM/state/details/aware_at.h>
#include <fQSM/state/slice/erased.h>

namespace fqsm::state {

    template<template <class> class InlaidContainer>
    struct Composite {
        struct Entry {
            using Erased = slice::Erased;

            template<aspect::Any Meta>
            using Typed = InlaidContainer<Meta>;

            template<aspect::Any Meta>
            using Handle = cref<Typed<Meta>>;

            template<aspect::Any Meta>
            using MutableHandle = ref<Typed<Meta>>;
        };

        const Schema schema;

        // --- typed (поверх erased) ---

        template<aspect::Any Meta>
        auto slice() const -> typename Entry::template Handle<Meta> {
            using Slice = typename Entry::template Typed<Meta>;
            return base::shared_ref_cast<const Slice>(
                slice(aspect::Rtid::of<Meta>())
            );
        }

        template<aspect::Any Meta>
        auto slice() -> typename Entry::template MutableHandle<Meta> {
            using Slice = typename Entry::template Typed<Meta>;
            return base::shared_ref_cast<Slice>(
                slice(aspect::Rtid::of<Meta>())
            );
        }

    protected:
        using Slices = std::unordered_map<aspect::Rtid, ref<typename Entry::Erased>, aspect::Rtid::Hash>;

        auto slice(aspect::Rtid runtimeTypeId) const -> cref<typename Entry::Erased> {
            return aware_at(slices, runtimeTypeId);
        }

        auto slice(aspect::Rtid runtimeTypeId) -> ref<typename Entry::Erased> {
            return aware_at(slices, runtimeTypeId);
        }

        Slices slices;
    };

}