#pragma once

#include <optional>
#include <base/maybe.h>
#include <iQSM/meta/alias.h>
#include <iQSM/state/_forwards.h>

namespace iqsm::state::detail {

    //template<typename Meta>
    //using FlatPatch = std::optional<Quantum<Meta>>;
    template<meta::Aspect Meta>
    struct FlatPatch : base::maybe<Quantum<Meta>> {
        bool is_noop() const { return exists(); }
    };

    template<meta::Aspect Meta>
    struct VersionedPatch {
        std::optional<Node<Meta>> before;
        std::optional<Node<Meta>> after;

        bool is_noop() const { return !before && !after; }
        bool is_add()  const { return !before &&  after; }
        bool is_del()  const { return  before && !after; }
        bool is_chg()  const { return  before &&  after; }
    };
    

}