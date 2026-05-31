#include "_common.h"
#include "_model.h"

#include <fQSM/api/interface.h>
#include <fQSM/state/world.h>

namespace tests {

void manipulators()
{
    using namespace ::tests::model;
    using namespace fqsm::api;
    
    const Schema schema = ask::schema::merge({
        ask::schema::aspect<SomeEntity>(),
    });

    //fqsm::World world = ask::world::create(schema);
    fqsm::state::world::Data world(schema);
    context::Realm main(world);

    const auto id = ask::item::create<SomeEntity>(main, {7});
    ask::item::update<SomeComponent>(main, id)->name = 6;


}

} // namespace tests
