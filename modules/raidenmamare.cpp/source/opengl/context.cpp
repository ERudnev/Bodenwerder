#include <opengl/context.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <base/logging.h>

namespace {
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
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) {
            glfwDestroyWindow(window);
            window = nullptr;
            glfwTerminate();
            return 3;
        }

        glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int width, int height) {
            glViewport(0, 0, width, height);
        });
        return 0;
    }
}

namespace rmmr::opengl {
    Context::Context(GLFWwindow* window)
        : window(window) {}

    Context::~Context() {
        if (!window) return;
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    auto Context::create() -> Ptr {
        GLFWwindow* window = nullptr;
        if (const int rc = initWindow(window); rc != 0) return {};
        return std::make_unique<Context>(window);
    }

    auto Context::getWindow(Provider provider, Core::Id id) -> GLFWwindow* {
        const auto* context = dynamic_cast<const Context*>(provider->layer<Core>()->get(id));
        if (!context) return nullptr;
        return context->window;
    }
}
