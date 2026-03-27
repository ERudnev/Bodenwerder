#include "_common.h"

#include <iQSM/api/_gateway.h>

namespace {
    // domain may (must) be defined with no "low level" my_math usage, making logical wrap
    namespace domain {

        using namespace iqsm::dsl_gateway;

        struct MathFunc : Handle<MathFunc>, Require<> {
            // please do not add default c-tor. Ever.
            struct Passport {
                std::string description; // user-defined name/key for the runtime functor
                integer cost;
            };

            struct Quantum {
                Passport passport;
                integer usage_cost; // domain business-logic
            };

            const Invariants invariants;

            struct Operations : OwnTypeOperations {
                // paid usage: mutates World (accumulates usage_cost using passport.cost)
                static void use(Writing, Id, resources::Manager, float arg) -> float;
                // free usage: pure read, does not mutate World
                static void use_free(Reading, Id, resources::Manager, float arg) -> float;
            };
        };

        // Impl of MathFunc (generally *.cpp file) so may include "my_math" types
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

    // this is like third-party, very external code, not allowed
    namespace my_math {
        using Function = std::function<float(float)>;
    }

    // deep part of domain (in general, resides in *.cpp file and not visible in domain interface)
    // this part depends on "low-level" domain "my_math"
    namespace domain {
        struct MathFuncHandle : iqsm::handle::Handle<MathFunc> {
            std::shared_ptr<my_math::Function> pointer;
        };

        iqsm::some_static_bind<MathFunc, MathFuncHandle>;

        // some template to bind MathFuncLoader as MathFunc runtime handler
    }
}

namespace tests {
    void handle_lifecycle() {
        // high-level things
        const auto schema = define_world_schema<MathFunc>();
        resources::Manager manager = define_as_select_resource_types_from_schema;
        repo::Branch master(world(schema));

        // Create and register handle + runtime resource in one step.
        // Important: the initial domain usage_cost is written explicitly (no default ctors).
        const MathFunc::Id function_id = manager.layer<MathFunc>()->create(
            master,
            MathFunc::Quantum{
                .passport = MathFunc::Passport{
                    .description = "sin(x)",
                    .cost = integer{10},
                },
                .usage_cost = integer{0},
            },
            my_math::Function([](float x) { return math::sin(x); })
        );

        auto paid_sin_of_twentyseven = MathFunc::Operations::use(master, function_id, manager, 27.0f);
        auto free_sin_of_ten = MathFunc::Operations::use_free(master, function_id, manager, 10.0f);

        // ops::handle::declare<MathFunc>(...)
    }
}

