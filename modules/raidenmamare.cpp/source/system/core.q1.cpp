#include <Raidenmamare/system/core.q1.h>

#include <GLFW/glfw3.h>

namespace rmmr::system {

    using namespace fqsm::api;

    struct Device::Internals : Device::DefaultInternals {
        static void release(Writing, Id, const Quantum& last) {
            if (last.handle) {
                glfwDestroyWindow(last.handle);
            }
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
