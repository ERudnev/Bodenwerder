#pragma once

#include <rmmr/system/core.q1.h>
#include <rmmr/system/window.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::system {

    using namespace fqsm::api;

    struct Interface : Manipulation<Interface, Core> {
        static auto create(Writing, string path, Core::GLVer version) -> Core::Id;
        static auto addDeviceAndWindow(Writing, Core::Id, decltype(Window::Quantum::title) title, index2 requested_size) -> Device::Id;
        static void shutdown(Writing);
    };

}
