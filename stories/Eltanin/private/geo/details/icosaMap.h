#pragma once

#include "geo/details/icosaPack.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace eltanin::geo {

    using namespace fqsm::api;

    template<typename T>
    struct IcosaMap {
        IcosaPack pack;
        vector<T> values;

        IcosaMap(IcosaPack, T fill);

        auto at(IcosaPack::Slot) -> T&;
        auto at(IcosaPack::Slot) const -> const T&;
        auto at(vec3 direction) const -> T requires (not std::is_integral_v<T>);
        auto at(IcosaPack::Tri) const -> T requires (not std::is_integral_v<T>);
        auto stitch() -> float;
    };

}

namespace eltanin::geo {

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
    auto IcosaMap<T>::at(vec3 direction) const -> T requires (not std::is_integral_v<T>) {
        return at(pack.triangle(IcosaPack::locate(direction)));
    }

    template<typename T>
    auto IcosaMap<T>::at(IcosaPack::Tri tri) const -> T requires (not std::is_integral_v<T>) {
        return tri.bary.x * at(tri.a) + tri.bary.y * at(tri.b) + tri.bary.z * at(tri.c);
    }

    // Last-resort weld: average stored copies of the same geometric vertex. Returns the total |stored - mean| moved.
    template<typename T>
    auto IcosaMap<T>::stitch() -> float {
        const integer last = pack.edgeSegments();
        if (last < 1)
            return 0.0f;
        pack.cacheWeld();
        using Acc = std::conditional_t<std::is_integral_v<T>, double, T>;
        auto asAcc = [](const T& value) -> Acc {
            if constexpr (std::is_integral_v<T>)
                return double(value);
            else
                return value;
        };
        float correction = 0.0f;
        for (const auto& group : pack.weld->groups) {
            if constexpr (std::is_same_v<T, std::uint16_t>) {
                // Covers pack two categorical facies IDs. Averaging the word can
                // carry into either byte and invent an out-of-range ID.
                T chosen = at(group.slots.front());
                std::size_t frequency = 0;
                for (const auto candidate : group.slots) {
                    const T value = at(candidate);
                    const std::size_t count = static_cast<std::size_t>(std::count_if(
                        group.slots.begin(), group.slots.end(),
                        [&](const auto slot) { return at(slot) == value; }));
                    if (count > frequency or (count == frequency and value < chosen)) {
                        chosen = value;
                        frequency = count;
                    }
                }
                for (const auto slot : group.slots) {
                    correction += float(at(slot) != chosen);
                    at(slot) = chosen;
                }
                continue;
            }
            Acc sum = asAcc(at(group.slots[0]));
            for (std::size_t copy = 1; copy < group.slots.size(); ++copy)
                sum = sum + asAcc(at(group.slots[copy]));
            const integer count = static_cast<integer>(group.slots.size());
            if constexpr (std::is_integral_v<T>) {
                const auto mean = static_cast<T>(std::lround(double(sum) / double(count)));
                for (const auto slot : group.slots) {
                    correction += float(std::abs(double(at(slot)) - double(mean)));
                    at(slot) = mean;
                }
            } else {
                const T mean = sum * (1.0f / float(count));
                for (const auto slot : group.slots) {
                    if constexpr (std::is_floating_point_v<T>)
                        correction += std::abs(at(slot) - mean);
                    else
                        correction += glm::length(at(slot) - mean);
                    at(slot) = mean;
                }
            }
        }
        return correction;
    }

}
