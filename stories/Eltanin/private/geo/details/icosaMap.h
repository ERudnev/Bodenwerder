#pragma once

#include "geo/details/icosaPack.h"

#include <glm/geometric.hpp>

#include <cmath>
#include <cstddef>
#include <type_traits>

namespace eltanin::geo {

    using namespace fqsm::api;

    enum class StitchMode { Average, PickFirst };

    template<typename T, StitchMode Mode = StitchMode::Average>
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

    template<typename T, StitchMode Mode>
    IcosaMap<T, Mode>::IcosaMap(IcosaPack pack, T fill)
        : pack(pack)
        , values(static_cast<std::size_t>(this->pack.storedCount()), fill) {
    }

    template<typename T, StitchMode Mode>
    auto IcosaMap<T, Mode>::at(IcosaPack::Slot slot) -> T& {
        return values[static_cast<std::size_t>(pack.index(slot))];
    }

    template<typename T, StitchMode Mode>
    auto IcosaMap<T, Mode>::at(IcosaPack::Slot slot) const -> const T& {
        return values[static_cast<std::size_t>(pack.index(slot))];
    }

    template<typename T, StitchMode Mode>
    auto IcosaMap<T, Mode>::at(vec3 direction) const -> T requires (not std::is_integral_v<T>) {
        return at(pack.triangle(IcosaPack::locate(direction)));
    }

    template<typename T, StitchMode Mode>
    auto IcosaMap<T, Mode>::at(IcosaPack::Tri tri) const -> T requires (not std::is_integral_v<T>) {
        return tri.bary.x * at(tri.a) + tri.bary.y * at(tri.b) + tri.bary.z * at(tri.c);
    }

    // Last-resort weld: make stored copies of the same geometric vertex agree.
    // Returns numeric displacement for averages or the number of changed copies when picking.
    template<typename T, StitchMode Mode>
    auto IcosaMap<T, Mode>::stitch() -> float {
        const integer last = pack.edgeSegments();
        if (last < 1)
            return 0.0f;
        pack.cacheWeld();
        float correction = 0.0f;
        for (const auto& group : pack.weld->groups) {
            if constexpr (Mode == StitchMode::PickFirst) {
                // Weld groups list slots in deterministic diamond order.
                const T chosen = at(group.slots.front());
                for (const auto slot : group.slots) {
                    correction += float(at(slot) != chosen);
                    at(slot) = chosen;
                }
            } else {
                using Acc = std::conditional_t<std::is_integral_v<T>, double, T>;
                auto asAcc = [](const T& value) -> Acc {
                    if constexpr (std::is_integral_v<T>)
                        return double(value);
                    else
                        return value;
                };
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
        }
        return correction;
    }

}
