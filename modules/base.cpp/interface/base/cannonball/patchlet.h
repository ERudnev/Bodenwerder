#pragma once

#include <utility>

namespace base::cannonball {

// Change carrier for one id.
// tombstone: explicit deletion — survives quantum edits; wins on integrate.
// verified: Scarlett — true after honest put_*; false after reference/touch (modify_modification).
template<typename T>
struct Patchlet {
    bool tombstone = false;
    bool verified = false;
    T quantum;

    static Patchlet deletion(T quantum) {
        return Patchlet{true, true, std::move(quantum)};
    }

    static Patchlet modification(T quantum) {
        return Patchlet{false, true, std::move(quantum)};
    }

    static Patchlet possible(T quantum) {
        return Patchlet{false, false, std::move(quantum)};
    }
};

} // namespace base::cannonball
