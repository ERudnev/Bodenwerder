#pragma once

#include <format>
#include <stdexcept>

#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/meta/rtid.h>

namespace fqsm::state {

    template<typename Map>
    auto aware_at(const Map& map, aspect::Rtid aspectId) -> const typename Map::mapped_type& {
        const auto found = map.find(aspectId);
        if (found == map.end()) {
            throw std::runtime_error(std::format(
                R"(aware_at: missing entry, aspect "{}")",
                aspect::Rtid::name(aspectId)));
        }
        return found->second;
    }

    template<aspect::Any Meta, typename Map>
    auto aware_at(const Map& map) -> const typename Map::mapped_type& {
        return aware_at(map, TypeId<Meta>);
    }

}
