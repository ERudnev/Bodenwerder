#pragma once

#include <functional>
#include <format>
#include <base/maybe.h>
#include <base/logging.h>

#include <iQSM/_forwards.h>

namespace iqsm::internals::repo {

    // commit is encapsulated transaction agent with restricted access only for short list of legal users
    // has initial state and gate to push somewhere own Delta.
    // !WARNING: designed to be never visible/usable outside of Permit/Transaction wraps

    // so, this structure has no encapsulation, because it is completely hidden by design
    struct Commit final {
        struct Result {
            base::maybe<Reading> maybeState;
            Delta delta;
        };
        using Upstream = std::function<void(Result)>;

        Reading state;
        Upstream upstream;

        Commit(const Commit&) = delete;
        Commit(Commit&&) = default;
        operator Reading() const { return state; }
        ~Commit() {
            if (upstream) {
                base::message("-commit: NOT USED");
            }
        }
        void kill() { upstream = {}; state.kill(); }

        void receive(Result result) {
            if (not upstream) {
                base::message("commit second call, rejecting");
                return;
            }            
            upstream(std::move(result)); // calling reveiver here!
            upstream = {};
        }
        
        Commit(Reading world, Upstream sink) : state(std::move(world)), upstream(std::move(sink)) {}        
    };

}