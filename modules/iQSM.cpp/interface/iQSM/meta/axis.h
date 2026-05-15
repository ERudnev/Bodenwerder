#pragma once

namespace iqsm::meta::axis {
    enum class versioning { // syntax: yep, small first character. enum class is not a type, it is namespace...
        shared,
        single,
    };

    enum class order { // in math terms verlet integration as "state := (0 * state + 1 * patch)". I am not kidding!
        state,
        patch,
    };
}