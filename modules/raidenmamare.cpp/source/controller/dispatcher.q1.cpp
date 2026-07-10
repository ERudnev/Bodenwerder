#include <Raidenmamare/controller/dispatcher.q1.h>

#include <GLFW/glfw3.h>

#include <cmath>

namespace rmmr::controller {

    using namespace fqsm::api;

    namespace {

        constexpr integer k_glfw_key_capacity = GLFW_KEY_LAST + 1;

        void poll_input(Writing context, system::Device::Id device) {
            const auto& device_quantum = with<system::Device>::get(context, device);
            GLFWwindow* const window = device_quantum.handle;
            if (not window) {
                return;
            }

            auto quantum = with<Dispatcher>::modify(context, device);
            if (quantum->keys.size() < static_cast<std::size_t>(k_glfw_key_capacity)) {
                quantum->keys.assign(static_cast<std::size_t>(k_glfw_key_capacity), false);
            }

            for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key) {
                quantum->keys[static_cast<std::size_t>(key)] = glfwGetKey(window, key) == GLFW_PRESS;
            }

            double mouse_x = 0.0;
            double mouse_y = 0.0;
            glfwGetCursorPos(window, &mouse_x, &mouse_y);
            quantum->mouse = index2{static_cast<integer>(std::lround(mouse_x)), static_cast<integer>(std::lround(mouse_y))};
        }

    } // namespace

    void Dispatcher::Actions::update(Writing context, Id device, seconds now) {
        if (not with<Dispatcher>::exists(context, device)) {
            with<Dispatcher>::extend(context, device, Dispatcher::Quantum{
                .clock = 0.0,
                .keys = {},
                .mouse = index2{0, 0},
            });
        }

        with<Dispatcher>::modify(context, device)->clock = now;
        poll_input(context, device);
    }

}
