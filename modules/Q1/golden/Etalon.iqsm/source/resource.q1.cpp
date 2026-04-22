#include <Etalon/resource.q1.h>

#include <cmath>
#include <format>
#include <stdexcept>
#include <unordered_map>

namespace Q1_iQSM::Etalon {
    struct SampleResource_private : SampleResource::Operations {
        using Functions = std::unordered_map<string, base::testing::function1f>;

        static auto functions() -> const Functions& {
            static const Functions value{
                { "sin(x)", [](float arg) { return std::sin(arg); } },
                { "cos(x)", [](float arg) { return std::cos(arg); } },
            };
            return value;
        }
    };


    void SampleResource::Materializer::materialize(resources::Manager manager, Reading world, Id id) const {
        if (manager->materialized<SampleResource>(id)) {
            throw std::runtime_error("SampleResource::Materializer::materialize: resource is already materialized");
        }

        const auto& passport = ops::particle::get<SampleResource>(world, id).passport;
        try {
            manager->layer<SampleResource>().materialize(id, SampleResource_private::functions().at(passport.description));
        } catch (const std::out_of_range&) {
            throw std::runtime_error(
                std::format(
                    "SampleResource::Materializer::materialize: unsupported passport description '{}'",
                    passport.description));
        }
    }

    void SampleResource::Materializer::release(resources::Manager manager, Reading, Id id) const {
        manager->layer<SampleResource>().release(id);
    }

    
    auto SampleResource::Operations::provide(Reading world, Id id) -> RuntimeAccess {
        return world->resources->layer<SampleResource>().provide(id);
    }

    auto SampleResource::Operations::use(Writing commit, Id id, float arg) -> float {
        repo::Accumulator acc{commit};
        ops::particle::modifier<SampleResource>(acc, id)->cost_summary += ops::particle::get<SampleResource>(acc, id).passport.cost;
        return provide(acc, id)(arg);
    }

    auto SampleResource::Operations::use_free(Reading world, Id id, float arg) -> float {
        return provide(world, id)(arg);
    }

    const Invariants SampleResource::invariants{
        .structural = {},
        .logical = {},
    };

} // namespace Q1_iQSM::Etalon