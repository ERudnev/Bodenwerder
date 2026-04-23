#include "_common.h"

#include <cmath>

#include <Etalon/resource.q1.h>

namespace tests {
    void resource_example() {
        using namespace iqsm::q1_gateway;
        using Q1_iQSM::Etalon::SampleResource;

        const auto schema = ops::schema::assemble<SampleResource>();
        resources::Manager manager = base::make_shared<iqsm::resources::ManagerCore>(schema);
        repo::Branch master(ops::world::create(schema, iqsm::freeze(manager)));

        const auto loaded_sin = ops::resource::create<SampleResource>(
            master,
            manager,
            SampleResource::Quantum{
                .passport = SampleResource::Materializer::Passport{
                    .description = "sin(x)",
                    .cost = 10,
                },
                .cost_summary = 0,
            },
            base::testing::function1f{ [](float arg) { return std::sin(arg); } }
        );

        const auto unloaded_cos = ops::resource::declare<SampleResource>(
            master,
            SampleResource::Quantum{
                .passport = SampleResource::Materializer::Passport{
                    .description = "cos(x)",
                    .cost = 10,
                },
                .cost_summary = 0,
            }
        );

        EXPECT_FALSE(ops::resource::materialized<SampleResource>(manager, unloaded_cos));

        ops::resource::materialize<SampleResource>(master, manager, unloaded_cos);

        EXPECT_TRUE(ops::resource::materialized<SampleResource>(manager, unloaded_cos));

        const auto provided_cos = ops::resource::provide<SampleResource>(master, unloaded_cos);
        const auto paid_sin = SampleResource::Operations::use(master, loaded_sin, 27.0f);
        const auto free_sin = SampleResource::Operations::use_free(master, loaded_sin, 10.0f);
        const auto free_cos = provided_cos(7.0f);

        EXPECT_TRUE(std::abs(paid_sin - std::sin(27.0f)) < 1e-5f);
        EXPECT_TRUE(std::abs(free_sin - std::sin(10.0f)) < 1e-5f);
        EXPECT_TRUE(std::abs(free_cos - std::cos(7.0f)) < 1e-5f);

        EXPECT_EQ(ops::particle::get<SampleResource>(master, loaded_sin).cost_summary, integer{10});
        EXPECT_EQ(ops::particle::get<SampleResource>(master, unloaded_cos).cost_summary, integer{0});

        ops::resource::release<SampleResource>(master, manager, unloaded_cos);

        EXPECT_FALSE(ops::resource::materialized<SampleResource>(manager, unloaded_cos));
    }
}
