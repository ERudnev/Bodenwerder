#pragma once

#include <iQSM/world.h>
#include <iQSM/delta.h>
#include <iQSM/operations/integration.h>

namespace iqsm::ops {
    struct Modifications {
        enum class Policy {
            accumulate,
            integrate, // keeps `current` in sync for dependent steps (not a "result world")
        };

        Modifications() = delete;
        explicit Modifications(Policy policy, World current) : policy(policy), current(current) {}

        Policy policy;
        World current;
        Delta accumulated = delta::empty(); // not applied to current yet
        Delta integrated = delta::empty();  // already applied to current

        void absorb(Delta update) {
            accumulated = merge(accumulated, update);
            if (policy == Policy::integrate) { integrate_pending(); }
        }

    private:
        void integrate_pending() {
            if (accumulated->empty()) return;
            current = ops::integrate(current, accumulated);
            integrated = merge(integrated, accumulated);
            accumulated = delta::empty();
        }
    };

    struct Context2 {
        World current() const { return modifications.current; }
        void absorb(Delta update);

    private:
        friend struct Transaction2;
        explicit Context2(Modifications& modifications) : modifications(modifications) {}
        Modifications& modifications;
    };

    struct Holder {
        Holder() = delete;
        explicit Holder(World world)
            : world(world) {}

        World current() const { return world; }
        void absorb(Delta delta) { world = ops::validate(ops::integrate(world, delta)); }
        void update(World integrated) { world = ops::validate(integrated); }

    private:
        World world;
    };

    struct Transaction2 {
        static Transaction2 accumulate(World world) { return Transaction2{world, Modifications::Policy::accumulate}; }
        static Transaction2 integrate(World world) { return Transaction2{world, Modifications::Policy::integrate}; }

        // allow passing Transaction2 to worker(Context2)
        operator Context2() { return Context2{modifications}; }

        World current() const { return modifications.current; }
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
        auto out = merge(modifications.integrated, modifications.accumulated);
        modifications.accumulated = delta::empty();
        modifications.integrated = delta::empty();
        return out;
    }

    inline void Transaction2::restart(World world) {
        modifications.current = world;
        modifications.accumulated = delta::empty();
        modifications.integrated = delta::empty();
        modifications.policy = base_policy;
    }
}
