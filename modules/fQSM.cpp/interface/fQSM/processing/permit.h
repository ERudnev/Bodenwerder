#pragma once

#include <stdexcept>
#include <memory>
#include <utility>

//#include <fQSM/processing/channel.h>
#include <fQSM/processing/_forwards.h>

// forwards:
namespace fqsm::processing { struct Transaction; }

namespace fqsm::processing {

    // transfers contexts between transactions
    struct Permit final {
        Permit(const Permit&) = delete;
        Permit(Permit& other) : transported( std::move(other.transported)) {}

    private:
        using Channel = ::fqsm::processing::Channel;
        friend struct ::fqsm::processing::Transaction;

        Permit(Channel& in) : transported(in) { in.reset(); }

        Channel consume() {
            if (!transported) {
                throw std::logic_error("fqsm::Writing: consumed permit");
            }
            return std::exchange(transported, nullptr);
        }
        //std::unique_ptr<Channel> transported;
        Channel transported;
    };


    /* old, overcomplicted stuff with IDEAL semantics...
    // std::move-like thing, passes Channel between transactions
    struct Permit final {
        Permit(const Permit&) = delete;
        Permit(Permit& other) : stolen((other.check_not_stolen(), std::move(other.stolen))) { other.stolen.kill(); }
        ~Permit() { 
            //if (not stolen.state.get())
            //    base::message("Permit: not transferred");
        }
    private:
        using Channel = ::fqsm::processing::Channel;
        friend struct ::fqsm::processing::Context;

        Permit(Channel&& channel) : stolen(std::move(channel)) {}

        void check_not_stolen() {
            if (not stolen.good()) throw std::logic_error("attempt to use killed Channel");
        }

        // payload:
        Channel stolen;
    };
    */
}