#pragma once

#include <fQSM/processing/channel.h>

namespace fqsm::processing {

    // std::move-like thing, passes Channel between transactions
    struct Permit final {
        Permit(const Permit&) = delete;
        Permit(Permit& other) : stolen((other.assert_not_stolen(), std::move(other.stolen))) { other.stolen.kill(); }
        ~Permit() { 
            //if (not stolen.state.get())
            //    base::message("Permit: not transferred");
        }
    private:
        using Channel = ::fqsm::processing::Channel;
        friend struct ::fqsm::processing::Context;
        Permit(Channel&& channel) : stolen(std::move(channel)) {}
    };
}