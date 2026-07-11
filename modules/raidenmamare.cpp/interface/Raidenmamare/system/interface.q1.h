#pragma once

#include <Raidenmamare/system/core.q1.h>
#include <Raidenmamare/system/window.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::system {

    using namespace fqsm::api;

    struct Interface : Manipulation<Interface, Core> {
        static auto create(Writing, string path, Core::GLVer version) -> Core::Id;
        static auto createWindow(Writing, decltype(Window::Quantum::title) title, decltype(Window::Quantum::size) size) -> Window::Id;
        static void shutdown(Writing);
    };

}
