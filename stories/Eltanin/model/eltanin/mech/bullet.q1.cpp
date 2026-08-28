#include <eltanin/mech/bullet.q1.h>

#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/family.q1.h>
#include <rmmr/scene/root.q1.h>

#include <cstddef>

namespace eltanin::mech {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        auto composeShell30mm(Writing context) -> optional<scene::actor::Family::Quantum> {
            const auto familyMaterial = with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("rmmr", "family"));
            if (not familyMaterial)
                return {};
            const auto pack = with<resource::Assets>::find<resource::meshpack::Asset>(context, resource::Unit::Name::from("Eltanin", "projectiles"));
            if (not pack)
                return {};
            auto resolved = with<resource::meshpack::Asset>::resolve(context, *pack, "shell_30mm");
            if (not resolved)
                return {};
            for (auto& entry : resolved->surfaces)
                entry.second = resource::material::Instance{.material = *familyMaterial, .textures = {}};
            resolved->texpack = {};
            return with<scene::actor::Family>::compose(context, std::move(*resolved), scene::actor::Family::Layout{.instanceBytes = 0, .fields = {}});
        }

    } // namespace

    void Bullet::Actions::bind(Writing context, scene::Root::Id root) {
        if (with<Bullet>::get_global(context).shell30mm) {
            context.refuse("eltanin::mech::Bullet::bind: already bound");
            return;
        }
        auto familyQuantum = composeShell30mm(context);
        if (not familyQuantum) {
            context.refuse("eltanin::mech::Bullet::bind: shell_30mm family compose failed");
            return;
        }
        with<Bullet>::modify_global(context)->shell30mm = with<scene::Interface>::createFamily(context, root, std::move(*familyQuantum));
    }

    auto Bullet::Actions::spawnShell30mm(Writing context, scene::Root::Id root, Pose pose, float speed) -> Id {
        const auto family = with<Bullet>::get_global(context).shell30mm;
        if (not family)
            return context.refuse("eltanin::mech::Bullet::spawnShell30mm: class is not bound");
        const auto bytes = with<scene::actor::Family>::get(context, *family).layout.instanceBytes;
        if (bytes < 0)
            return context.refuse("eltanin::mech::Bullet::spawnShell30mm: Family.layout.instanceBytes is negative");
        scene::actor::Packed packed(static_cast<std::size_t>(bytes));
        const auto replica = with<scene::Interface>::createReplica(context, root, *family, pose, scene::actor::Replica::Quantum{.family = *family, .packed = std::move(packed)});
        if (not with<scene::actor::Replica>::exists(context, replica))
            return context.refuse("eltanin::mech::Bullet::spawnShell30mm: replica create failed");
        return with<Bullet>::create(context, Bullet::Quantum{.actor = replica, .speed = speed});
    }

    auto Bullet::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Bullet, scene::actor::Replica, &Bullet::Quantum::actor>{},
        };
    }

}
