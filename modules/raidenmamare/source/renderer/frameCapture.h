#pragma once

#include <filesystem>

#include <fQSM/api/interface.h>
#include <rmmr/system/window.q1.h>

namespace rmmr::renderer {

    bool captureBackBuffer(fqsm::api::Reading context, system::Window::Id window, std::filesystem::path destination);

}
