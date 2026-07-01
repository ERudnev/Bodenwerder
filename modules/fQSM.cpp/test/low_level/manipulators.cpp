#include "_common.h"
#include "minimodel/aspects.q1.h"

#include <fQSM/api/interface.h>

namespace tests {

void manipulation()
{
    using namespace ::tests::model;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<SomeEntity>(),
        ask::schema::aspect<SomeComponent>(),
    });

    context::Realm main(schema);

    { // create + get (standalone)
        const auto id = with<archetype::EntWithComp>::spawn(main, 7, "seven");
        EXPECT_EQ(ask::item::get<SomeEntity>(main, id)->value, 7);
        EXPECT_EQ(debug::count<SomeEntity>(main), 1);
    }

    { // create + get (parasitic)
        const auto id = with<archetype::EntWithComp>::spawn(main, 42, "hello");
        EXPECT_TRUE(ask::item::get<SomeComponent>(main, id).exists());
        EXPECT_EQ(ask::item::get<SomeComponent>(main, id)->name, "hello");
        EXPECT_EQ(debug::count<SomeComponent>(main), 2);
    }

    { // update (modify): requires existing component, commits on dtor
        const auto id = with<archetype::EntWithComp>::spawn(main, 1, "before");

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
