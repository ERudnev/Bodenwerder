#include <Raidenmamare/core.q1.h>

#include <GLFW/glfw3.h>

#include <base/logging.h>

namespace rmmr {

    namespace {
        struct OpenglContext final : iqsm::binding::resource::Data {
            GLFWwindow* window = nullptr;

            explicit OpenglContext(GLFWwindow* window)
                : window(window) {}

            ~OpenglContext() override {
                if (!window) return;
                glfwDestroyWindow(window);
                glfwTerminate();
            }
        };

        auto initWindow(GLFWwindow*& window) -> int {
            if (!glfwInit()) return 1;

            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

            window = glfwCreateWindow(800, 600, "OpenGL Hello World Triangle", nullptr, nullptr);
            if (!window) {
                glfwTerminate();
                return 2;
            }

            glfwMakeContextCurrent(window);
            glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int width, int height) {
                glViewport(0, 0, width, height);
            });
            return 0;
        }
    }

    struct OpenglContextLoader final : iqsm::binding::resource::Loader<Core> {
        iqsm::binding::resource::Ptr load(iqsm::World world, iqsm::binding::resource::Manager, Core::Id id) const override {
            const auto& passport = ops::particle::get<Core>(world, id).passport;
            (void)passport;

            GLFWwindow* window = nullptr;
            if (const int rc = initWindow(window); rc != 0) {
                return {};
            }

            return std::make_unique<OpenglContext>(window);
        }

        void unload(iqsm::World, iqsm::binding::resource::Manager, Core::Id, iqsm::binding::resource::Data& data) const override {
            auto* context = dynamic_cast<OpenglContext*>(&data);
            if (!context) throw ::base::detail::make_incomplete_message(__FILE__, __LINE__, __func__);
            if (!context->window) return;

            glfwDestroyWindow(context->window);
            context->window = nullptr;
            glfwTerminate();
        }
    };

    auto tryGetWindow(const iqsm::binding::resource::Data& data) -> GLFWwindow* {
        const auto* context = dynamic_cast<const OpenglContext*>(&data);
        if (!context) return nullptr;
        return context->window;
    }

    auto getWindow(iqsm::binding::resource::Manager manager, Core::Id id) -> GLFWwindow* {
        return tryGetWindow(*manager->layer<Core>()->get(id));
    }

    auto coreLoader() -> const iqsm::binding::resource::Loader<Core>& {
        static const OpenglContextLoader loader{};
        return loader;
    }

    const Invariants Core::invariants{
        .structural = {},
        .logical = {},
    };
}
