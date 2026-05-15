#pragma once

#include <stdexcept>

#include <iQSM/flow/internals/channel.h>

namespace iqsm {
    namespace flow::internals { struct Permit; }
    namespace flow { struct Transaction; }
    using Writing = flow::internals::Permit;
}

namespace iqsm::flow::internals {

    /*
    Permit is not a value object. It is a one-shot mandate for mutation.

    Semantics intentionally differ from ordinary C++ ownership types:
    1. Passing Permit further means consuming the right to write, not copying data.
    2. Permit exists to keep call sites clean: user code passes transactions naturally,
       without repetitive std::move noise at every relay point.
    3. The consumed state is not an accident of implementation; it is the meaning of the type.
    4. Reusing the same Permit is a logic error, because one mandate must produce one write path.
    5. The "stealing" constructor is deliberate: lvalue transfer here models capability passing,
       not ordinary object copying.
    6. If a caller needs several independent write paths, it must construct an explicit transaction
       axis (Branch / Sequence / Accumulator / Staged), not duplicate Permit.
    7. Permit is internal transport between transactions and helpers; ergonomic surface is prioritized
       over conventional C++ move syntax in this narrow runtime-specific case.
    */
    struct Permit final{
        //operator Reading() const { assert_not_stolen(); return static_cast<Reading>(stolen); }

        //auto operator->() const { assert_not_stolen(); return stolen.state.operator->(); }

        Permit(const Permit&) = delete;
        Permit(Permit& other) : stolen((other.assert_not_stolen(), std::move(other.stolen))) { other.stolen.kill(); }
        ~Permit() { 
            //if (not stolen.state.get())
            //    base::message("Permit: not transferred");
        }

        //  EXPERIMENT... Permit(Permit&&) =  default;

    private:
        using Channel = internals::Channel;
        friend struct ::iqsm::flow::Transaction; // Permit is just a transport between Transactions
        //Permit(Channel& channel) : stolen(std::move(channel)) { base::message("Permit c-tor(Channel): stolen channel");}
        Permit(Channel&& channel) : stolen(std::move(channel)) {}
        inline void assert_not_stolen() const {
            if (not stolen.state.get()) throw std::logic_error("iqsm::Writing: attempted to use consumed permit");
            //if (not stolen.state.get()) base::message("critical: using write Permit 2nd time");
        }

        Channel stolen;
    };    
}