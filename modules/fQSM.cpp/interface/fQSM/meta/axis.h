#pragma once

namespace fqsm::meta::axis {
    enum class order { // in math terms verlet integration as "state := (0 * state + 1 * patch)". I am not kiddin!
        state,
        patch,
    };
}