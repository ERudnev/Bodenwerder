#pragma once

#include <stdexcept>

#include <iQSM/_forwards.h>
#include <iQSM/repository/commit.h>

namespace iqsm {
    namespace repo { struct Permit; }
    using Writing = repo::Permit;
}

namespace iqsm::repo {

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
       policy (Branch / Sequence / Accumulator / Staged), not duplicate Permit.
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
        using Commit = internals::repo::Commit;
        friend struct Transaction; // Permit is just a transport between Transactions
        //Permit(Commit& commit) : stolen(std::move(commit)) { base::message("Permit c-tor(Commit): stolen commit");}
        Permit(Commit&& commit) : stolen(std::move(commit)) {}
        inline void assert_not_stolen() const {
            if (not stolen.state.get()) throw std::logic_error("iqsm::Writing: attempted to use consumed permit");
            //if (not stolen.state.get()) base::message("critical: using write Permit 2nd time");
        }

        Commit stolen;
    };    
}