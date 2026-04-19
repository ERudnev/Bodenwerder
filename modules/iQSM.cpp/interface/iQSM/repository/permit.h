#pragma once

#include <stdexcept>

#include <iQSM/_forwards.h>
#include <iQSM/repository/commit.h>

namespace iqsm {
    namespace repo { struct Permit; }
    using Writing = repo::Permit;
    using Reading = World;
}

namespace iqsm::repo {

    // it is a "mandate for only one update". Forces to define strategy in function where acts as parameter
    // implemented as "Thief" - steals (moves-out) commit to provoke creating Branch for more than 1 writing op.
    struct Permit {
        operator Reading() const { assert_not_stolen(); return static_cast<Reading>(stolen); }

        auto operator->() const { assert_not_stolen(); return stolen.state.operator->(); }

        Permit(const Permit&) = delete;
        Permit(Permit& other) : stolen((other.assert_not_stolen(), std::move(other.stolen))) { other.stolen.kill(); }
        //  EXPERIMENT... Permit(Permit&&) =  default;

        // cliend may discard own transaction, disarming "commit: not used" warning
        void discard() {
            assert_not_stolen();
            stolen.kill();
        }

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