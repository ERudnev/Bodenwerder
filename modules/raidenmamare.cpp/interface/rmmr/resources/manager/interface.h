#pragma once

#include <fQSM/processing/persistency/schema.h>

#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/shadows.q1.h>
#include <rmmr/resources/textures.q1.h>

namespace rmmr::assets {

    namespace persist = fqsm::processing::persistency;

    // High-level Engine face of the asset catalogue (types to save/load).
    // Binder: class template Aspect<Meta> convertible to persist::Schema
    // (e.g. persistency::json::Aspect / persistency::database::Aspect).
    struct Interface {
        template<template<typename> typename Aspect>
        static auto persistencySchema() -> persist::Schema {
            // Unit_group is Engine ownership — not archived; load does not mount into it.
            // Workbench: only material::Asset while persistency for nested techniques is reshaped.
            static const auto schema = persist::merge({
                // Aspect<resource::Unit>{},
                // Aspect<resource::texture::Asset>{},
                // Aspect<resource::texture::Loader>{},
                // Aspect<resource::texture::Generator>{},
                // Aspect<resource::shader::Asset>{},
                // Aspect<resource::shader::Loader>{},
                Aspect<resource::material::Asset>{},
                // Aspect<resource::material::Composer>{},
                // Aspect<resource::shadow::Asset>{},
                // Aspect<resource::shadow::Allocator>{},
                // Aspect<resource::geometry::Asset>{},
                // Aspect<resource::geometry::Loader>{},
                // Aspect<resource::geometry::Generator>{},
            });
            return schema;
        }
    };

}
