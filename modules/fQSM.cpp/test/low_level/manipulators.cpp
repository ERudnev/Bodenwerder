#include "_common.h"
#include "minimodel/aspects.q1.h"

#include <fQSM/api/interface.h>
#include <fQSM/state/world/data.h>

namespace tests {

void manipulation()
{
    using namespace ::tests::model;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<SomeEntity>(),
        ask::schema::aspect<SomeComponent>(),
    });

    fqsm::state::world::Data world(schema);
    context::Realm main(world);

    { // create + get (standalone)
        const auto id = ask::item::create<SomeEntity>(main, {7});
        EXPECT_EQ(ask::item::get<SomeEntity>(main, id)->value, 7);
        EXPECT_EQ(debug::count<SomeEntity>(main), 1);
    }

    { // create + get (parasitic)
        const auto id = ask::item::create<SomeEntity>(main, {42});
        EXPECT_FALSE(ask::item::get<SomeComponent>(main, id).exists());

        ask::item::create<SomeComponent>(main, id, {"hello"});
        EXPECT_TRUE(ask::item::get<SomeComponent>(main, id).exists());
        EXPECT_EQ(ask::item::get<SomeComponent>(main, id)->name, "hello");
        EXPECT_EQ(debug::count<SomeComponent>(main), 1);
    }

    { // update (modify): requires existing component, commits on dtor
        const auto id = ask::item::create<SomeEntity>(main, {1});
        ask::item::create<SomeComponent>(main, id, {"before"});

        {
            auto tx = ask::item::update<SomeComponent>(main, id);
            tx->name = "after";
        }

        EXPECT_EQ(ask::item::get<SomeComponent>(main, id)->name, "after");
    }

    { // update without precondition: modify ctor must fail
        const auto id = ask::item::create<SomeEntity>(main, {99});
        bool threw = false;
        try {
            ask::item::update<SomeComponent>(main, id);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        EXPECT_TRUE(threw);
    }
}

} // namespace tests
