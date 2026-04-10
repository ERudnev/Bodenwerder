#include <Raidenmamare/core.q1.h>

#include <opengl/context.h>

#include <base/logging.h>

namespace rmmr {
    struct OpenglContextLoader final : iqsm::binding::resource::Loader<Core> {
        iqsm::binding::resource::Ptr load(iqsm::World world, iqsm::binding::resource::Manager, Core::Id id) const override {
            const auto& passport = ops::particle::get<Core>(world, id).passport;
            (void)passport;
            return opengl::Context::create();
        }

        void unload(iqsm::World, iqsm::binding::resource::Manager, Core::Id, iqsm::binding::resource::Data& data) const override {
            const auto* context = dynamic_cast<const opengl::Context*>(&data);
            if (!context) _THROW_LOGIC_ERROR_;
            (void)context;
        }
    };

    auto coreLoader() -> const iqsm::binding::resource::Loader<Core>& {
        static const OpenglContextLoader loader{};
        return loader;
    }

    const Invariants Core::invariants{
        .structural = {},
        .logical = {},
    };
}
