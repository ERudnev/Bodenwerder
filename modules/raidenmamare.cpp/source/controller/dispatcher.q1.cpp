#include <Raidenmamare/controller/dispatcher.q1.h>
#include <Raidenmamare/device.q1.h>

#include <GLFW/glfw3.h>

#include <cmath>

#include <iQSM/api/_gateway_equal.h>
#include <iQSM/helpers/global.h>
#include <iQSM/repository/transactions/sequence.h>

namespace rmmr::controller {

    struct Dispatcher_private : Dispatcher::Operations {
        static void applyInput(Writing);
    };

    void Dispatcher_private::applyInput(Writing permit) {
        repo::Sequence transaction(permit);
        const auto& device = ops::global::get<Dispatcher>(transaction)->device;
        if (not device) {
            return;
        }
        GLFWwindow* const w = Device::Operations::provide(transaction, *device);
        if (!w) {
            return;
        }

        auto mod = ops::global::modifier<Dispatcher>(transaction);
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

    void Dispatcher::Operations::update(Writing permit, seconds now_sec) {
        iqsm::repo::Sequence transaction(permit);
        ops::global::modifier<Dispatcher>(transaction)->clock = now_sec;
        Dispatcher_private::applyInput(transaction);
    }

    const Invariants Dispatcher::invariants{
        .structural = {},
        .logical = {},
    };
}
