#include "_common.h"
#include "_model.h"

#include <fQSM/api/interface.h>
#include <fQSM/state/world.h>

namespace {
    using namespace ::tests::model;
    using namespace fqsm::api;

    void experimental_manipulator(fqsm::Writing context, int number, std::string str) {
        context::GrouppedOperations tx(context);
        const auto id = ask::item::add<SomeEntity>(tx, { .value = number });
        ask::item::add<SomeComponent>(tx, id, { .name = str });
    }
}


namespace tests {
    using namespace ::tests::model;
    // this test is TDD framework to envolve fQSM World
    void flat_model_assembly()
    {
        using namespace fqsm::api;
        const Schema schema = ask::schema::merge({
            ask::schema::aspect<SomeEntity>(),
            ask::schema::aspect<SomeComponent>(),
        });

        //fqsm::World world = ask::world::create(schema);
        fqsm::state::world::Data world(schema);
        context::Realm main(world);

        // temp, RnD:
        { // RnD: create 10 workers, call one by one and then merge results into main
            std::vector<context::Sequence> workers;
            for (int xx = 0; xx < 10; ++xx)
                workers.emplace_back(context::Branch(main));

            // while each worker does its job, actual world state in realm is constant, so it may work in MT environment
            for (auto& wrk: workers)
                experimental_manipulator(wrk);

            // this must integrate all worker-hosted changes. Each worker result integration
            workers.clear(); // this will call desctructors of all worker contexts and call sequence of integrations
        }
    }
}

