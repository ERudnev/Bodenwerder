#include "materialGenerator.h"

#include <Raidenmamare/materials/program.q1.h>

#include <utility>

namespace rmmr::material {
    namespace {

        Core::Id create_preset(fqsm::Writing context, Window::Id window, string program_name, string core_name, string library, Uniform::Palette uniforms) {
            const auto program = with<Program_group>::addElement(context, window, Program::Quantum{
                .name = std::move(program_name),
                .library = std::move(library),
                .handle = {},
            });
            with<Program>::compile(context, program, window);

            const auto core = with<Core>::create(context, Core::Quantum{
                .name = std::move(core_name),
                .program = program,
                .uniforms = std::move(uniforms),
            });
            with<Core>::compile(context, core, window);
            return core;
        }

    } // namespace

    auto MaterialGenerator::ambient(fqsm::Writing context, Window::Id window) -> Core::Id {
        return create_preset(context, window, "ambient", "ambient", "rmmr",
            Core::Actions::uniformIds(vector<string>{
                "model",
                "view",
                "projection",
                "albedo",
                "ambientColor",
                "ambientIntensity",
            }));
    }

    auto MaterialGenerator::lit(fqsm::Writing context, Window::Id window) -> Core::Id {
        return create_preset(context, window, "lit", "lit", "rmmr",
            Core::Actions::uniformIds(vector<string>{
                "model",
                "view",
                "projection",
                "albedo",
                "ambientColor",
                "ambientIntensity",
                "light0Pos",
                "light0Color",
                "light0Intensity",
            }));
    }

    auto MaterialGenerator::grid(fqsm::Writing context, Window::Id window) -> Core::Id {
        return create_preset(context, window, "Grid", "grid", "rmmr",
            Core::Actions::uniformIds(vector<string>{
                "model",
                "view",
                "projection",
                "patternScale",
                "colorPrimary",
                "colorSecondary",
            }));
    }
}
