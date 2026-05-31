#pragma once

#include <format>
#include <stdexcept>

#include <fQSM/meta/alias.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/meta/runtimeId.h>

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
        const auto aspectId = aspect::Rtid::of<Meta>();
        const auto found = map.find(aspectId);
        if (found == map.end()) {
            throw std::runtime_error(std::format(
                R"(aware_at: missing entry, aspect "{}")",
                aspect::Rtid::name(aspectId)));
        }
        return found->second;
    }

}
