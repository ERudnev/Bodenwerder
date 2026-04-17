#pragma once

#include "commit.h"

namespace iqsm_mock::repo {

    // it is a "mandate for only one update". Forces to define strategy in function where acts as parameter
    // implemented as "Thief" - steals (moves-out) commit to provoke creating Branch for more than 1 writing op.
    struct Permit {
        operator Reading() const { return static_cast<Reading>(stolen); }

        Permit(const Permit&) = delete;
        Permit(Permit& other) noexcept : stolen(std::move(other.stolen)) { base::message("Permit c-tor(Permit): stolen target commit"); }
        Permit(Permit&&) =  default;

    private:
        friend struct Transaction; // Permit is just a transport between Transactions
        //Permit(Commit& commit) : stolen(std::move(commit)) { base::message("Permit c-tor(Commit): stolen commit");}
        Permit(Commit&& commit) : stolen(std::move(commit)) { base::message("Permit c-tor(Commit): stolen commit");}
        
        Commit stolen;
    };

    using Writing = Permit;
    
}