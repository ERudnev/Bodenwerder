#pragma once

#include <iQSM/world.h>
#include <iQSM/delta.h>
#include <iQSM/operations/integration.h>
#include <iQSM/logger.h>

namespace iqsm::ops {
    struct Modifications {
        enum class Policy {
            ignore,
            accumulate,
            sync, // keeps `pre_cursor` in sync for dependent steps (not a "result world")
        };

        Modifications() = delete;
        explicit Modifications(Policy policy, World pre_cursor) : policy(policy), pre_cursor(pre_cursor) {}

        Policy policy;
        World pre_cursor;
        Delta accumulated = delta::empty(); // not applied to pre_cursor yet
        Delta integrated = delta::empty();  // already applied to pre_cursor

        void absorb(Delta update) {
            if (policy == Policy::ignore) {
                logger::message("Transaction2: absorb ignored (policy=ignore)");
                return;
            }
            accumulated = merge(accumulated, update);
            if (policy == Policy::sync) { integrate_pending(); }
        }

    private:
        void integrate_pending() {
            if (accumulated->empty()) return;
            pre_cursor = ops::integrate(pre_cursor, accumulated);
            integrated = merge(integrated, accumulated);
            accumulated = delta::empty();
        }
    };

    struct Context2 {
        World pre_cursor() const { return modifications.pre_cursor; }
        void absorb(Delta update);

    private:
        friend struct Transaction2;
        explicit Context2(Modifications& modifications) : modifications(modifications) {}
        Modifications& modifications;
    };

    struct Transaction2 {
        static Transaction2 accumulate(World world) { return Transaction2{world, Modifications::Policy::accumulate}; }
        static Transaction2 sync(World world) { return Transaction2{world, Modifications::Policy::sync}; }

        // allow passing Transaction2 to worker(Context2)
        operator Context2() { return Context2{modifications}; }

        World pre_cursor() const { return modifications.pre_cursor; }
        void absorb(Delta update);
        [[nodiscard]] Delta finish();
        void restart(World world);

    private:
        explicit Transaction2(World world, Modifications::Policy policy) : modifications(policy, world), base_policy(policy) {}

        Modifications modifications;
        Modifications::Policy base_policy;
    };

    inline void Context2::absorb(Delta update) { modifications.absorb(update); }

    inline void Transaction2::absorb(Delta update) {
        modifications.absorb(update);
    }

    inline Delta Transaction2::finish() {
        // one-shot: returns raw diff and disables further absorb until restart()
        auto out = merge(modifications.integrated, modifications.accumulated);
        modifications.accumulated = delta::empty();
        modifications.integrated = delta::empty();
        modifications.policy = Modifications::Policy::ignore;
        return out;
    }

    inline void Transaction2::restart(World world) {
        modifications.pre_cursor = world;
        modifications.accumulated = delta::empty();
        modifications.integrated = delta::empty();
        modifications.policy = base_policy;
    }
}
