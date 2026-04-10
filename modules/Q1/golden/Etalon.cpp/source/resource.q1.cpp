#include <Etalon/resource.q1.h>

#include <cmath>
#include <format>
#include <stdexcept>
#include <unordered_map>

namespace Q1CORE::Etalon {
    struct SampleResourcePrivate : SampleResource::Operations {
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
            manager->layer<SampleResource>().materialize(id, SampleResourcePrivate::functions().at(passport.description));
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

    
    auto SampleResource::Operations::provide(Reading, Id id, resources::Manager manager) -> RuntimeAccess {
        return manager->layer<SampleResource>().provide(id);
    }

    auto SampleResource::Operations::use(Writing commit, Id id, resources::Manager manager, float arg) -> float {
        const Quantum& q = ops::particle::get<SampleResource>(commit.initial, id);
        ops::particle::modifier<SampleResource>(commit, id)->cost_summary += q.passport.cost;
        return provide(commit.initial, id, manager)(arg);
    }

    auto SampleResource::Operations::use_free(Reading world, Id id, resources::Manager manager, float arg) -> float {
        return provide(world, id, manager)(arg);
    }

    const Invariants SampleResource::invariants{
        .structural = {},
        .logical = {},
    };

} // namespace Q1CORE::Etalon