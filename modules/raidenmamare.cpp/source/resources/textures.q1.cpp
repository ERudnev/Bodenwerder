#include <rmmr/resources/runtimes.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace rmmr::resource::texture {

    using namespace fqsm::api;

    struct Runtime::Internals : Runtime::DefaultInternals {
        static void release(Writing context, Id runtime_id, const Quantum& last) {
            if (not last.handle) {
                return;
            }

            glfwMakeContextCurrent(with<system::Device>::get(context, last.device).handle);
            auto handle = last.handle;
            glDeleteTextures(1, &handle);
        }
    };

    auto Runtime::customAspectReactions() -> const Behavior {
        return {
            reaction::deletion<Runtime>(&Runtime::Internals::release),
        };
    }

}
