#include "cluster/astronomy.h"
#include "cluster/measure.h"

#include <eltanin/cluster/starmap/details.q1.h>

#include <base/logging.h>

namespace eltanin::cluster {

    using namespace fqsm::api;
    using space::Pose;
    using space::Position;

    void Astronomy::generate(Writing context) {
        axes.clear();
        celestials.clear();
        const Pose pose{.position = Position{0.0, 0.0, 0.0}, .orientation = dquat{1.0, 0.0, 0.0, 0.0}};
        const auto grain = with<Axis>::create(context, Axis::Quantum{.pose = pose});
        with<starmap::Details>::extend(context, grain, starmap::Details::Quantum{.name = "Sol"});
        with<Celestial>::extend(context, grain, Celestial::Quantum{
            .position = Position{0.0, 0.0, 0.0},
            .mass = measure::Mass::sun * measure::celestialFactor * measure::celestialFactor,
            .radius = float(measure::Radius::sun * measure::celestialFactor),
        });
        with<Star>::extend(context, grain, Star::Quantum{.temperature = measure::Temperature::sun, .color = vec3{1.00f, 0.95f, 0.90f}});
        axes.push_back(grain);
        celestials.push_back(grain);
        base::message("eltanin::cluster::Astronomy: Sol");
    }

}
