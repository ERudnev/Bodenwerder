#pragma once

namespace eltanin::mech::slot {

    enum class inner {
        multi, // it is extension for any ajacent frame of any kind. "Free Type"
        engine, // assumes free back (or other engine slot)
        power,
        gyros,
        hardpoint, // assumes top or front open space
        hangar, // assumes
        cargo,
        logistic, // slot is virtually free space to form "crossroads" between modules (new against Red Star)
        emissive,
        control,
        living,
    };

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

    enum class wing {
        radiance,
        agfe,
        pylon,
    };
}