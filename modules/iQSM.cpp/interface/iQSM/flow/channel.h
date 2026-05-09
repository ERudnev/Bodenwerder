#pragma once

#include <functional>
#include <format>
#include <base/maybe.h>
#include <base/logging.h>
#include <iQSM/flow/reading.h>
#include <iQSM/state/interface.h>

namespace iqsm::internals::flow {

    // channel is encapsulated transaction agent with restricted access only for short list of legal users
    // has initial state and gate to push somewhere own Delta.
    // !WARNING: designed to be never visible/usable outside of Permit/Transaction wraps

    // so, this structure has no encapsulation, because it is completely hidden by design
    struct Channel final {
        struct Result {
            base::maybe<Reading> maybeState;
            Delta delta;
        };
        using Upstream = std::function<void(Result)>;

        Reading state;
        Upstream upstream;

        Channel(const Channel&) = delete;
        Channel(Channel&&) = default;
        operator Reading() const { return state; }
        ~Channel() {
            if (upstream) {
                base::message("-channel: NOT USED");
            }
        }
        void kill() { upstream = {}; state.kill(); }

        void receive(Result result) {
            if (not upstream) {
                base::message("channel second receive(), rejecting");
                return;
            }            
            upstream(std::move(result)); // calling reveiver here!
            upstream = {};
        }
        
        Channel(Reading world, Upstream sink) : state(std::move(world)), upstream(std::move(sink)) {}        
    };

}