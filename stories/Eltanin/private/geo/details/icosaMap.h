#pragma once

#include "geo/details/icosaPack.h"

#include <glm/geometric.hpp>

#include <cmath>
#include <map>
#include <type_traits>

namespace eltanin::locality::geo {

    using namespace fqsm::api;

    template<typename T>
    struct IcosaMap {
        IcosaPack pack;
        vector<T> values;

        IcosaMap(IcosaPack, T fill);

        auto at(IcosaPack::Slot) -> T&;
        auto at(IcosaPack::Slot) const -> const T&;
        auto at(vec3 direction) const -> T;
        auto stitch() -> float;
    };

}

namespace eltanin::locality::geo {

    template<typename T>
    IcosaMap<T>::IcosaMap(IcosaPack pack, T fill)
        : pack(pack)
        , values(static_cast<std::size_t>(this->pack.storedCount()), fill) {
    }

    template<typename T>
    auto IcosaMap<T>::at(IcosaPack::Slot slot) -> T& {
        return values[static_cast<std::size_t>(pack.index(slot))];
    }

    template<typename T>
    auto IcosaMap<T>::at(IcosaPack::Slot slot) const -> const T& {
        return values[static_cast<std::size_t>(pack.index(slot))];
    }

    template<typename T>
    auto IcosaMap<T>::at(vec3 direction) const -> T {
        const IcosaPack::Tri tri = pack.triangle(IcosaPack::locate(direction));
        return tri.bary.x * at(tri.a) + tri.bary.y * at(tri.b) + tri.bary.z * at(tri.c);
    }

    // Last-resort weld: average stored copies of the same geometric vertex. Returns the total |stored - mean| moved.
    template<typename T>
    auto IcosaMap<T>::stitch() -> float {
        const integer last = pack.edgeSegments();
        if (last < 1)
            return 0.0f;
        struct Key {
            integer first;
            integer second;
            integer along;
            auto operator<(const Key& other) const -> bool {
                if (first != other.first)
                    return first < other.first;
                if (second != other.second)
                    return second < other.second;
                return along < other.along;
            }
        };
        struct Group {
            T sum;
            integer count;
            vector<IcosaPack::Slot> slots;
        };
        std::map<Key, Group> groups;
        auto keyOf = [&](IcosaPack::Slot slot) -> Key {
            const auto& corners = IcosaPack::diamonds()[slot.diamond];
            const bool west = slot.iu == 0;
            const bool east = slot.iu == last;
            const bool north = slot.iv == 0;
            const bool south = slot.iv == last;
            if (west and north)
                return Key{.first = corners.top, .second = -1, .along = 0};
            if (east and north)
                return Key{.first = corners.right, .second = -1, .along = 0};
            if (west and south)
                return Key{.first = corners.left, .second = -1, .along = 0};
            if (east and south)
                return Key{.first = corners.bottom, .second = -1, .along = 0};
            integer from = 0;
            integer to = 0;
            integer along = 0;
            if (north) {
                from = corners.top;
                to = corners.right;
                along = slot.iu;
            } else if (west) {
                from = corners.top;
                to = corners.left;
                along = slot.iv;
            } else if (east) {
                from = corners.right;
                to = corners.bottom;
                along = slot.iv;
            } else {
                from = corners.left;
                to = corners.bottom;
                along = slot.iu;
            }
            if (from > to)
                return Key{.first = to, .second = from, .along = last - along};
            return Key{.first = from, .second = to, .along = along};
        };
        auto consider = [&](IcosaPack::Slot slot) {
            const Key key = keyOf(slot);
            if (auto found = groups.find(key); found != groups.end()) {
                found->second.sum = found->second.sum + at(slot);
                found->second.count += 1;
                found->second.slots.push_back(slot);
                return;
            }
            groups.emplace(key, Group{.sum = at(slot), .count = 1, .slots = {slot}});
        };
        for (integer diamond = 0; diamond < IcosaPack::diamondCount; ++diamond) {
            for (integer iu = 0; iu <= last; ++iu) {
                consider(IcosaPack::Slot{.diamond = diamond, .iu = iu, .iv = 0});
                consider(IcosaPack::Slot{.diamond = diamond, .iu = iu, .iv = last});
            }
            for (integer iv = 1; iv < last; ++iv) {
                consider(IcosaPack::Slot{.diamond = diamond, .iu = 0, .iv = iv});
                consider(IcosaPack::Slot{.diamond = diamond, .iu = last, .iv = iv});
            }
        }
        float correction = 0.0f;
        for (const auto& entry : groups) {
            const T mean = entry.second.sum * (1.0f / float(entry.second.count));
            for (const auto slot : entry.second.slots) {
                if constexpr (std::is_floating_point_v<T>)
                    correction += std::abs(at(slot) - mean);
                else
                    correction += glm::length(at(slot) - mean);
                at(slot) = mean;
            }
        }
        return correction;
    }

}
