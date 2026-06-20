#include "_common.h"
#include "minimodel/aspects.q1.h"

#include <fQSM/api/interface.h>

namespace {
    using namespace ::tests::model;
    using namespace fqsm::api;

    void experimental_manipulator(fqsm::Writing context, int number, std::string str) {
        const auto id = ask::item::create<SomeEntity>(context, { .value = number });
        ask::item::create<SomeComponent>(context, id, { .name = str });
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
            ask::schema::aspect<SecondaryAttribute>(),
        });

        //fqsm::World world = ask::world::create(schema);
        fqsm::state::world::Data world(schema);
        context::Realm main(world);

        const auto id = ask::item::create<SomeEntity>(main, {7});
        ask::item::update<SomeComponent>(main, id, {"seven"});
        {
            auto tx = ask::item::update<SecondaryAttribute>(main, id,
                {static_cast<integer>(ask::item::get<SomeComponent>(main, id)->name.length())}
            );
            tx->attribute = 88;
            tx->attribute = 77;
        }

    }
}
