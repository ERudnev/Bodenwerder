#pragma once

#include <glm/vec3.hpp>

namespace eltanin::mech::slot {

    enum class inner {
        multi, // it is extension for any ajacent frame of any kind. "Free Type"
        engine, // assumes free back (or other engine slot)
        power,
        battery,
        hardpoint, // assumes top or front open space
        hangar, // assumes
        cargo,
        logistic, // slot is virtually free space to form "crossroads" between modules (new against Red Star)
        emissive,
        control,
        living,
    };

    // Actor albedo tint for shared white inner mesh (texture × albedo).
    inline auto color(inner role) -> glm::vec3 {
        switch (role) {
            case inner::multi: return {0.70f, 0.70f, 0.72f};
            case inner::engine: return {0.95f, 0.35f, 0.15f};
            case inner::power: return {0.95f, 0.85f, 0.20f};
            case inner::battery: return {0.20f, 0.85f, 0.90f};
            case inner::hardpoint: return {0.90f, 0.20f, 0.22f};
            case inner::hangar: return {0.65f, 0.35f, 0.90f};
            case inner::cargo: return {0.72f, 0.52f, 0.30f};
            case inner::logistic: return {0.30f, 0.55f, 0.95f};
            case inner::emissive: return {0.95f, 0.30f, 0.70f};
            case inner::control: return {0.25f, 0.85f, 0.40f};
            case inner::living: return {0.95f, 0.70f, 0.55f};
        }
        return {1.0f, 1.0f, 1.0f};
    }

    enum class plate {
        armor, // looks as default plate type
        thruster, // typically it is rear panel of any engine frame
        cooling, // typically is appears as side panel of engine frame
        turret, // this is side (top) panel for hardpoints, when possible
        barrel, // this is front panel of "front-mounted" gun or launcher
        pd, // this lot for panel allows attachment of point-defence, NOT related to hardpoints
        hatch, // this is "door for hangar" but may vary
        bay, // a "door for cargo", opens from a side of logistic blocks
        antenna, // any "emissive" volume shold hawe as many as possible panels of this kind
        cockpit, // allows "control" frame to have nice-looking window
        windowed, // living frame should have it aside, or crewmen will go crazy
        agfe, // anti-gravity-force-emitter panel at the bottom of ship (new against RedStar)
        utility, // mount for any low-power stuff: barrels, small external tanks, decoration
        logistic, // extends shupp hull out of frame to add coridirs, smal locks, service tunnels
    };

}
