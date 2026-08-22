#include <rmmr/system/core.q1.h>

#include <GLFW/glfw3.h>

#include <base/logging.h>

namespace rmmr::system {

    using namespace fqsm::api;

    auto Core::Actions::singleton(Reading context) -> Id {
        return with<Core>::get_global(context).singleton;
    }

    auto Core::Actions::access(Reading context) -> const Quantum* {
        return &with<Core>::get(context, singleton(context));
    }

    auto Clock::Actions::singleton(Reading context) -> Id {
        return with<Clock>::get_global(context).singleton;
    }

    struct Device::Internals : Device::DefaultInternals {
        static void release(Writing, Id id, const Quantum& last) {
            (void)id;
            (void)last;
        }
    };

    void Device::Actions::poll_events(Reading) {
        glfwPollEvents();
    }

    auto Device::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::anchored<Device, Core, &Device::Quantum::core>{},
            reaction::deletion<Device>(&Internals::release),
        };
    }

}
