#pragma once

#include <functional>
#include <format>
#include <base/logging.h>

#include "../environment/iqsm_root.h"

namespace iqsm_mock::repo {

    using Reading = iqsm_mock::World;

    // commit is encapsulated transaction agent with restricted access only for short list of legal users
    // has initial state and gate to push somewhere own Delta.
    // !WARNING: designed to be never visible/usable outside of Permit/Transaction wraps

    // so, this structure has no encapsulation, because it is completely hidden by design
    struct Commit final {
        using Upstream = std::function<void(Delta)>;

        iqsm_mock::World state;
        Upstream upstream;

        Commit(const Commit&) = delete;
        Commit(Commit&&) = default;
        operator Reading() const { return state; }
        ~Commit() { base::message(std::format("-commit: {}", upstream ? "NOT USED" : "used"));}

        void receive(Delta delta) {
            base::message("commit: received Delta");
            if (not upstream) {
                base::message("commit SECOND call, rejecting");
                return;
            }            
            upstream(std::move(delta)); // calling reveiver here!
            upstream = {};
        }
        
        Commit(World world, Upstream sink) : state(std::move(world)), upstream(std::move(sink)) {
            base::message("+commit");
        }        
    };

}