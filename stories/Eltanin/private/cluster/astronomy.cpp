#include "cluster/astronomy.h"
#include "cluster/measure.h"

#include <eltanin/cluster/starmap/details.q1.h>

#include <base/logging.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace eltanin::cluster {

    using namespace fqsm::api;
    using space::Pose;
    using space::Position;
    using chemistry::Volatile;

    namespace {

        auto solarMix() -> Volatile::Mix {
            Volatile::Mix mix{};
            mix = Volatile::pack(mix, Volatile::Kind::Hydrogen, 15);
            mix = Volatile::pack(mix, Volatile::Kind::Helium, 5);
            mix = Volatile::pack(mix, Volatile::Kind::Oxygen, 1);
            mix = Volatile::pack(mix, Volatile::Kind::Carbon, 1);
            mix = Volatile::pack(mix, Volatile::Kind::Silicon, 1);
            mix = Volatile::pack(mix, Volatile::Kind::Iron, 1);
            return mix;
        }

        auto placeStar(Writing context, Position position, string name, double solarMasses, Volatile::Mix mix) -> Axis::Id {
            const double mass = Mass::sun * solarMasses;
            const float radius = float(Radius::sun * std::pow(solarMasses, 0.8));
            const float temperature = Temperature::sun * float(std::pow(solarMasses, 0.55));
            const float age = std::min(10000.0f * float(std::pow(solarMasses, -2.5)) * 0.46f, 13000.0f);
            const auto grain = with<Axis>::create(context, Axis::Quantum{.pose = Pose{.position = position, .orientation = dquat{1.0, 0.0, 0.0, 0.0}}});
            with<starmap::Details>::extend(context, grain, starmap::Details::Quantum{.name = std::move(name)});
            with<Celestial>::extend(context, grain, Celestial::Quantum{.position = Position{0.0, 0.0, 0.0}, .mass = mass, .radius = radius});
            with<Star>::extend(context, grain, Star::Quantum{.temperature = temperature, .mix = mix, .age = age});
            return grain;
        }

    }

    void Astronomy::generateSol(Writing context, Position position) {
        const auto grain = placeStar(context, position, "Sol", 1.0, solarMix());
        axes.push_back(grain);
        celestials.push_back(grain);
    }

    void Astronomy::generate(Writing context) {
        axes.clear();
        celestials.clear();
        constexpr integer wings = 5;
        constexpr double step = 5.0 * eLY;
        const auto mix = solarMix();
        for (integer index = -wings; index <= wings; ++index) {
            const Position position{double(index) * step, 0.0, 0.0};
            if (index == 0) {
                generateSol(context, position);
                continue;
            }
            const double solarMasses = std::pow(10.0, 0.18 * double(index));
            const auto grain = placeStar(context, position, std::format("MS {:+d} ly", index * 5), solarMasses, mix);
            axes.push_back(grain);
            celestials.push_back(grain);
        }
        base::message("eltanin::cluster::Astronomy: main sequence {}", axes.size());
    }

}
