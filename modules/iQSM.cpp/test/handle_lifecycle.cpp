#include "_common.h"

#include <cmath>

#include <Etalon/resource.q1.h>
#include "_utilities/function1f_loader.h"


namespace tests {
    void handle_lifecycle() {
        using namespace iqsm::dsl_gateway;
        using Q1CORE::Etalon::SampleResource;
        using Function1f = tests::resources::Function1f;
        const tests::resources::Function1fLoader function_loader{};

        const auto schema = ops::schema::assemble<SampleResource>();
        iqsm::dsl_gateway::resources::Manager manager = base::make_shared<iqsm::binding::ManagerData>();
        repo::Branch master(ops::world::create(schema));

        // generation of new resource instance:
        const SampleResource::Id new_function_sin = ops::resource::create<SampleResource>(
            master,
            manager,
            SampleResource::Quantum{
                .passport = SampleResource::Passport{
                    .description = "sin(x)",
                    .cost = 10,
                },
                .cost_summary = 0,
            },
            std::make_unique<Function1f::Data>(
                Function1f::PayloadType{ [](float x) { return std::sin(x); } }
            )
        );

        // simulate situation when Binding present, but resource is not loaded yet:
        const auto old_function_cos = ops::resource::declare<SampleResource>(
            master,
            SampleResource::Quantum{
                .passport = SampleResource::Passport{
                    .description = "cos(x)",
                    .cost = 10,
                },
                .cost_summary = 0,
            }
        );
        // ensure that resource is not loaded yet:
        EXPECT_EQ(false, ops::resource::loaded<SampleResource>(manager, old_function_cos));

        // "loading" existing resource with known Passport
        const bool success = ops::resource::load<SampleResource>(
            master,
            manager,
            old_function_cos,
            function_loader
        );
        EXPECT_TRUE(success);

        const auto paid_sin_of_twentyseven = SampleResource::Operations::use(master, new_function_sin, manager, 27.0f);
        const auto free_sin_of_ten = SampleResource::Operations::use_free(master, new_function_sin, manager, 10.0f);
        const auto free_cos_of_seven = SampleResource::Operations::use_free(master, old_function_cos, manager, 7.0f);

        EXPECT_TRUE(std::abs(paid_sin_of_twentyseven - std::sin(27.0f)) < 1e-5f);
        EXPECT_TRUE(std::abs(free_sin_of_ten - std::sin(10.0f)) < 1e-5f);
        EXPECT_TRUE(std::abs(free_cos_of_seven - std::cos(7.0f)) < 1e-5f);

        EXPECT_EQ(ops::particle::get<SampleResource>(master, new_function_sin).cost_summary, integer{10});
        EXPECT_EQ(ops::particle::get<SampleResource>(master, old_function_cos).cost_summary, integer{0});

        // ops::binding::declare<SampleResource>(...)
    }
}

