#include "model.h"

#include <memory>
#include <stdexcept>

#include <iQSM/_all.include.h>

// domain:
#include <Atomic/varph.q1.h>

namespace Toy {

    using namespace Q1CORE::Example::Varph;
    // TODO: consider making this as "tables" later
    struct Settings {
        struct ParticleType {
            integer charge;
            eVt mass;
        };

        inline static const ParticleType proton{integer{1}, eVt{900}};
        inline static const ParticleType neutron{integer{0}, eVt{901}};
        inline static const ParticleType electron{integer{-1}, eVt{1}};

        static const ParticleType& by_name(std::string_view name) {
            if (name == "proton") return proton;
            if (name == "neutron") return neutron;
            if (name == "electron") return electron;
            throw std::runtime_error("Toy::Settings: unknown particle type");
        }
    };


    //
    void Model::create() {
        using namespace iqsm::logger;
        using namespace Q1CORE::Example::Varph;
        using namespace iqsm;

        constexpr int w = 16;
        constexpr int h = 16;
        constexpr float step = 1.0f;
        constexpr eVt locality = eVt{0};

        const auto schema = ops::schema::assemble<Inertia, Charge>();
        std::string aspect_names;
        for (const auto& kv : schema->aspects) { aspect_names += (aspect_names.empty() ? "" : ", ") + kv.second.name; }
        message("Toy::Model::create(): schema aspects ({}) = [{}]", schema->aspects.size(), aspect_names);
        auto accumulator = repo::Accumulator{world};

        const float clock = 0.0f;
        ops::global::modifier<Spark>(accumulator)->clock = clock;

        for (int z = 0; z < h; ++z) {
            for (int x = 0; x < w; ++x) {
                const float fx = (static_cast<float>(x) - (static_cast<float>(w - 1) * 0.5f)) * step;
                const float fz = (static_cast<float>(z) - (static_cast<float>(h - 1) * 0.5f)) * step;

                const auto position = vec4{fx, 0.0f, fz, clock};

                const std::string_view type_name = ((x + z) % 3 == 0) ? "proton" : (((x + z) % 3 == 1) ? "neutron" : "electron");
                const auto& type = Settings::by_name(type_name);

                const auto id = ops::particle::create<Spark>(accumulator, Spark::Quantum{position, locality});
                ops::particle::create<Inertia>(accumulator, id, Inertia::Quantum{position, type.mass});
                ops::particle::create<Charge>(accumulator, id, Charge::Quantum{type.charge});
            }
        }

        world = ops::validate_smart(world, ops::integrate(world, accumulator.push()));
    }

    void Model::loadFromFile() {
        // TODO: bind to filesystem API; load from fileBinding into world.
    }
}