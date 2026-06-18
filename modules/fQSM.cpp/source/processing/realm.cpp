
#include <fQSM/processing/transactions/realm.h>

#include <fQSM/state/patch.h>
#include <fQSM/processing/actions/normalization.h>

namespace fqsm::processing {
    auto Realm::writing() -> Writing {
        auto patch = base::make_shared<model::complex::Patch>(world.schema);
        auto context = std::make_shared<Context>(Context{
            world,
            patch,
            [this](Context::PatchRef patch) {
                accept(patch);
            }
        });

        return GateWriting{world, context};
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
        lastNotes = {};
        lastNotes = actions::update(world, *patch);
    }

    void Realm::accept_immediate(aspect::Rtid type) {
        auto patch = model::complex::Patch(world.schema);
        patch.composite().slices.emplace(type, world.schema->nodes.at(type).binding.createDirtyVirtualPatch());

        lastNotes = {};
        lastNotes = actions::update(world, patch);
    }
}
