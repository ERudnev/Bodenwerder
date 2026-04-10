#pragma once

#include <Raidenmamare/program.q1.h>

#include <GL/glew.h>

#include <filesystem>

#include <iQSM/binding/resource.h>

namespace rmmr::opengl {
    struct Program final : iqsm::binding::resource::Data {
        using Data = iqsm::binding::resource::Data;
        using Ptr = iqsm::binding::resource::Ptr;
        using Provider = iqsm::binding::resource::Provider;

        GLuint handle = 0;

        explicit Program(GLuint handle);
        ~Program() override;

        static auto create(const std::filesystem::path&, const rmmr::Program::Passport&) -> Ptr;
        static auto getHandle(Provider, rmmr::Program::Id) -> GLuint;
    };
}
