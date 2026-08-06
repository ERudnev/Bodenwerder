#pragma once

#include <rmmr/renderer/gl.q1.h>
#include <rmmr/system/core.q1.h>

#include <cstdint>

#include <fQSM/api/interface.h>

namespace rmmr::system {

    using namespace fqsm::api;

    struct Window : Component<Window, Device> {
        enum class Presentation : std::uint8_t {
            windowed,
            maximized,
        };
        struct InputState {
            vector<bool> keys;
            vector<bool> buttons;
            index2 mouse;
            renderer::Integer32 under;
        };

        struct Quantum {
            string title;
            InputState previous;
            InputState current;
            integer identityDraws;
        };
        struct Actions : BaseActions {
            static auto create(Writing, string title, index2 requested_size, Presentation presentation) -> Id;
            static auto framebufferSize(Reading, Id) -> index2;
            static void present(Reading, Id);
            static auto mouseShift(Reading, Id) -> index2;
            static void onFrameAdvanced(Writing, Id);
            // Call after ImGuiHost::newFrame. Clears keys / mouse delta+buttons when UI wants them.
            static void applyUiCapture(Writing, Id);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
