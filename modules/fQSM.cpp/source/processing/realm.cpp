/*
#include <fQSM/processing/transactions/realm.h>

#include <fQSM/state/patch.h>
#include <fQSM/processing/actions/normalization.h>

namespace fqsm::processing {
    auto Realm::writing() -> Writing {
        auto patch = base::make_shared<state::world::Patch>(world.schema);
        auto context = std::make_shared<Commit>(Commit{
            world,
            patch,
            [this](Commit::PatchRef patch) {
                accept(patch);
            }
        });

        return GateWriting{world, context};
    }

    auto Realm::makeChildPolicy() -> ChildPolicy {
        return ChildPolicy{
            world,
            [this](Commit::PatchRef patch) {
                accept(patch);
            }
        };
    }

    void Realm::accept(Commit::PatchRef patch) {
        lastNotes = {};
        lastNotes = actions::update(world, *patch);
    }

    void Realm::accept_immediate(aspect::Rtid type) {
        auto patch = state::world::Patch(world.schema);
        patch.composite().slices.emplace(type, world.schema->nodes.at(type).binding.createDirtyVirtualPatch());

        lastNotes = {};
        lastNotes = actions::update(world, patch);
    }
}
*/