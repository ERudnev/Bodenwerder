#include <rmmr/system/core.q1.h>

#include <GLFW/glfw3.h>

#include <base/logging.h>

namespace rmmr::system {

    using namespace fqsm::api;

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
