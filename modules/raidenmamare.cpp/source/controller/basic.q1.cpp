#include <Raidenmamare/controller/basic.q1.h>
#include <Raidenmamare/device.q1.h>

#include <GLFW/glfw3.h>

#include <cmath>

#include <iQSM/api/_gateway_equal.h>
#include <iQSM/helpers/global.h>
#include <iQSM/repository/sequence.h>

namespace rmmr::controller {

    struct Core_private : Core::Operations {
        static void applyInput(Writing step);
    };

    void Core_private::applyInput(Writing step) {
        const auto& device = ops::global::get<Core>(step.initial)->device;
        if (not device) {
            return;
        }
        GLFWwindow* const w = Device::Operations::provide(step.initial, *device);
        if (!w) {
            return;
        }

        auto mod = ops::global::modifier<Core>(step);
        auto& keys = mod->keys;

        if (keys.size() < static_cast<size_t>(GLFW_KEY_LAST + 1)) {
            keys.assign(static_cast<size_t>(GLFW_KEY_LAST + 1), false);
        }

        for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key) {
            keys[static_cast<size_t>(key)] = glfwGetKey(w, key) == GLFW_PRESS;
        }

        double mx = 0.0;
        double my = 0.0;
        glfwGetCursorPos(w, &mx, &my);
        mod->mouse = index2{static_cast<integer>(std::lround(mx)), static_cast<integer>(std::lround(my))};
    }

    void Core::Operations::update(Writing outer, seconds now_sec) {
        iqsm::repo::Sequence seq{outer.initial};
        ops::global::modifier<Core>(seq)->clock = now_sec;
        Core_private::applyInput(seq);
        outer.push(seq.push());
    }

    const Invariants Core::invariants{
        .structural = {},
        .logical = {},
    };
}
