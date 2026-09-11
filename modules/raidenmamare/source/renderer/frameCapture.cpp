#include "frameCapture.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <system_error>
#include <vector>

#include <GL/glew.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../../3party/glfw/deps/stb_image_write.h"

namespace rmmr::renderer {

    bool captureBackBuffer(fqsm::api::Reading context, system::Window::Id window, std::filesystem::path destination) {
        const auto size = system::Window::Actions::framebufferSize(context, window);
        const auto width = static_cast<int>(size.x);
        const auto height = static_cast<int>(size.y);
        if (width <= 0 or height <= 0 or destination.empty())
            return false;

        constexpr int channels = 4;
        const auto row_bytes = static_cast<std::size_t>(width) * channels;
        std::vector<std::uint8_t> pixels(row_bytes * static_cast<std::size_t>(height));

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadBuffer(GL_BACK);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glFinish();
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        // OpenGL starts at the lower-left corner; PNG rows start at the top.
        for (int top = 0, bottom = height - 1; top < bottom; ++top, --bottom) {
            auto top_row = pixels.begin() + static_cast<std::ptrdiff_t>(top) * static_cast<std::ptrdiff_t>(row_bytes);
            auto bottom_row = pixels.begin() + static_cast<std::ptrdiff_t>(bottom) * static_cast<std::ptrdiff_t>(row_bytes);
            std::swap_ranges(top_row, top_row + static_cast<std::ptrdiff_t>(row_bytes), bottom_row);
        }

        std::error_code error;
        if (const auto parent = destination.parent_path(); not parent.empty()) {
            std::filesystem::create_directories(parent, error);
            if (error)
                return false;
        }

        return stbi_write_png(destination.string().c_str(), width, height, channels, pixels.data(), width * channels) != 0;
    }

}
