#pragma once

#include <Raidenmamare/core.q1.h>

#include <iQSM/binding/resource.h>

struct GLFWwindow;

namespace rmmr::opengl {
    struct Context final : iqsm::binding::resource::Data {
        using Data = iqsm::binding::resource::Data;
        using Ptr = iqsm::binding::resource::Ptr;
        using Provider = iqsm::binding::resource::Provider;

        GLFWwindow* window = nullptr;

        explicit Context(GLFWwindow* window);
        ~Context() override;

        static auto create() -> Ptr;
        static auto getWindow(Provider, Core::Id) -> GLFWwindow*;
    };
}
