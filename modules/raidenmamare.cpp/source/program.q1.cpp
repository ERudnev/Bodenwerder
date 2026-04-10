#include <Raidenmamare/program.q1.h>

#include <opengl/context.h>
#include <opengl/program.h>

#include <base/logging.h>

#include <filesystem>

namespace rmmr {
    struct OpenglProgramLoader final : iqsm::binding::resource::Loader<Program> {
        iqsm::binding::resource::Ptr load(iqsm::World world, iqsm::binding::resource::Manager manager, Program::Id id) const override {
            const auto& quantum = ops::particle::get<Program>(world, id);
            const auto& passport = quantum.passport;
            const auto& corePassport = ops::particle::get<Core>(world, quantum.core).passport;
            if (!opengl::Context::getWindow(manager, quantum.core)) return {};

            const std::filesystem::path assetRoot(corePassport.assets_root);
            return opengl::Program::create(assetRoot, passport);
        }

        void unload(iqsm::World, iqsm::binding::resource::Manager, Program::Id, iqsm::binding::resource::Data& data) const override {
            auto* program = dynamic_cast<opengl::Program*>(&data);
            if (!program) _THROW_LOGIC_ERROR_;
            if (!program->handle) return;

            glDeleteProgram(program->handle);
            program->handle = 0;
        }
    };

    auto programLoader() -> const iqsm::binding::resource::Loader<Program>& {
        static const OpenglProgramLoader loader{};
        return loader;
    }

    const Invariants Program::invariants{
        .structural = {},
        .logical = {},
    };
}
