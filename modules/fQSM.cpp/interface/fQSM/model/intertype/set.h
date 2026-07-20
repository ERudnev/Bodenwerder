#pragma once

#include <algorithm>
#include <iterator>
#include <set>

#include <fQSM/meta/interface.include.h>

namespace fqsm::model::intertype {

    // Algebraic set of aspect type ids (palette, sources, ...).
    // Public inheritance from std::set keeps initializer_list / iteration / insert cheap;
    // Set adds only the algebra we actually speak in fQSM.
    struct Set : std::set<meta::Rtid> {
        using Base = std::set<meta::Rtid>;
        using Base::Base;
        using Base::contains;

        template<meta::category::Any Meta>
        bool contains() const {
            return Base::contains(TypeId<Meta>);
        }

        auto operator|=(const Set& other) -> Set& {
            insert(other.begin(), other.end());
            return *this;
        }

        auto operator&=(const Set& other) -> Set& {
            Set kept;
            std::set_intersection(
                begin(), end(),
                other.begin(), other.end(),
                std::inserter(kept, kept.end())
            );
            swap(kept);
            return *this;
        }

        friend auto operator|(Set lhs, const Set& rhs) -> Set {
            lhs |= rhs;
            return lhs;
        }

        friend auto operator&(Set lhs, const Set& rhs) -> Set {
            lhs &= rhs;
            return lhs;
        }
    };

}
