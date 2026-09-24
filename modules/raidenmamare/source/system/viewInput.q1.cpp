#include <rmmr/system/viewInput.q1.h>

#include <vector>

namespace rmmr::system {

    using namespace fqsm::api;

    auto ViewInput::Actions::create(Writing context) -> Id {
        return ViewInput::BaseActions::create(context, ViewInput::Quantum{
            .engaged = false,
            .keys = {},
            .buttons = {},
            .mouseShift = index2{0, 0},
            .wheel = 0.0f,
        });
    }

    void ViewInput::Actions::engage(Writing context, Id id, bool on) {
        with<ViewInput>::modify(context, id)->engaged = on;
    }

    void ViewInput::Actions::refresh(Writing context, Window::Id window) {
        const auto& device = with<Window>::get(context, window);
        const auto shift = with<Window>::mouseShift(context, window);
        vector<Id> ids;
        for (const auto& entry : context->aspect<ViewInput>().items())
            ids.push_back(entry.id);
        for (const auto id : ids) {
            auto mail = with<ViewInput>::modify(context, id);
            if (not mail->engaged) {
                mail->keys.clear();
                mail->buttons.clear();
                mail->mouseShift = index2{0, 0};
                mail->wheel = 0.0f;
                continue;
            }
            mail->keys = device.current.keys;
            mail->buttons = device.current.buttons;
            mail->mouseShift = shift;
            mail->wheel = device.current.wheel;
        }
    }

}
