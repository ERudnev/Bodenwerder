#include <fQSM/processing/transactions/realm.h>

#include <fQSM/processing/actions/integration.h>

namespace fqsm::processing {
    auto Realm::writing() -> Writing {
        auto patch = base::make_shared<state::world::Patch>(world.schema);
        auto context = std::make_shared<Context>(Context{
            world,
            patch,
            [this](Context::PatchRef patch) {
                accept(patch);
            }
        });

        return Gate{world, context};
    }

    auto Realm::makeChildPolicy() -> ChildPolicy {
        return ChildPolicy{
            world,
            [this](Context::PatchRef patch) {
                accept(patch);
            }
        };
    }

    void Realm::accept(Context::PatchRef patch) {
        actions::integrate(world, *patch);
    }
}