#pragma once

#include <cstdint>
#include <imgui.h>

#include <fQSM/api/interface.h>

// rearrange whole ui/ folfer (draft state)
namespace rmmr::ui {

    namespace detail {

        inline int compressed_raw(std::uint64_t raw) {
            std::uint64_t x = raw;
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
            x = x ^ (x >> 31);
            return static_cast<int>(static_cast<std::uint32_t>(x));
        }

        template<typename Meta>
        int compressed_id(const typename Meta::Id& id) {
            return compressed_raw(id.raw());
        }

    } // namespace detail

    template<typename Meta>
    void push_entity_id(const typename Meta::Id& id) {
        ImGui::PushID(detail::compressed_id<Meta>(id));
    }

}
