#include <fQSM/processing/realm.h>

#include <fQSM/processing/algorithm/integration.h>

namespace fqsm::processing {
    Permit Realm::createFork() {
        auto patch = base::make_shared<state::world::Patch>(world.schema);

        Channel newChannel = std::make_shared<Context>(
            world,
            patch,
            [this](Context::PatchRef patch) {
                update(patch);
            }
        );

        return grantPermit(newChannel);
    }

    void Realm::update(Context::PatchRef patch) {
        integration::integrate(world, *patch);
    }
}