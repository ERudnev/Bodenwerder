#include "_common.h"

#include <cmath>
#include <functional>
#include <type_traits>
#include <utility>

#include <iQSM/api/_gateway.h>
#include <iQSM/schema.h>







// domain may (must) be defined with no "low level" my_math usage, making logical wrap
namespace domain {

    using namespace iqsm::dsl_gateway;

    struct MathFunc : Binding<MathFunc>, Require<> {
        // please do not add default c-tor. Ever.
        struct Passport {
            std::string description; // user-defined name/key for the runtime functor
            integer cost;
        };

        struct Quantum {
            Passport passport;
            integer usage_cost; // domain business-logic
        };
        struct Global {};

        static const Invariants invariants;

        struct Operations : OwnTypeOperations {
            // paid usage: mutates World (accumulates usage_cost using passport.cost)
            static auto use(Writing, Id, resources::Manager, float arg) -> float;
            // free usage: pure read, does not mutate World
            static auto use_free(Reading, Id, resources::Manager, float arg) -> float;
        };
    };

} // end of header part


// this is like third-party, very external code, not allowed
namespace my_math {
    using Function = std::function<float(float)>;
}

// deep part of domain (in general, resides in *.cpp file and not visible in domain interface)
// this part depends on "low-level" domain "my_math"
namespace domain {
    struct MathFuncHandle : iqsm::detail::binding::Driver {
        my_math::Function function;

        template<typename F> requires(!std::is_same_v<std::decay_t<F>, MathFuncHandle>)
        explicit MathFuncHandle(F&& f) : function(std::forward<F>(f)) {}

        float apply(float x) const { return function(x); }
    };

    // some template to bind MathFuncLoader as MathFunc runtime handler
}

// Impl of MathFunc (generally *.cpp file) so may include "my_math" types
namespace iqsm::detail::binding {
    template<>
    struct driver_of<domain::MathFunc> {
        using type = domain::MathFuncHandle;  // или как у вас называется конкретный драйвер
    };
}

namespace domain {

    inline auto MathFunc::Operations::use(Writing commit, Id id, resources::Manager manager, float arg) -> float {
        const Quantum& q = ops::particle::get<MathFunc>(commit.initial, id);
        // Domain bookkeeping: charge for usage using per-instance passport.cost.
        ops::particle::modifier<MathFunc>(commit, id)->usage_cost += q.passport.cost;
        // Runtime call (external resource): obtain functor by id and evaluate.
        return manager->layer<MathFunc>()->get(id)->apply(arg);
    }

    inline auto MathFunc::Operations::use_free(Reading, Id id, resources::Manager manager, float arg) -> float {
        return manager->layer<MathFunc>()->get(id)->apply(arg);
    }

    const Invariants MathFunc::invariants{
        .structural = {},
        .logical = {},
    };
}

namespace tests {
    void handle_lifecycle() {
        using namespace iqsm::dsl_gateway;
        using domain::MathFunc;
        using domain::MathFuncHandle;
        // high-level things
        const auto schema = ops::schema::assemble<MathFunc>();
        resources::Manager manager = base::make_shared<iqsm::binding::ManagerData>();
        manager->register_layer<MathFunc>();
        repo::Branch master(ops::world::create(schema));

        // Create and register handle + runtime resource in one step.
        // Important: the initial domain usage_cost is written explicitly (no default ctors).
        const MathFunc::Id function_id = manager->layer<MathFunc>()->create(
            master,
            MathFunc::Quantum{
                .passport = MathFunc::Passport{
                    .description = "sin(x)",
                    .cost = 10,
                },
                .usage_cost = 0,
            },
            MathFuncHandle{ [](float x) { return std::sin(x); } }
        );

        auto paid_sin_of_twentyseven = MathFunc::Operations::use(master, function_id, manager, 27.0f);
        auto free_sin_of_ten = MathFunc::Operations::use_free(master, function_id, manager, 10.0f);

        // ops::binding::declare<MathFunc>(...)
    }
}

