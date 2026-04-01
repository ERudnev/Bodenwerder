#include <Etalon/resource.q1.h>

#include <stdexcept>

#include <base/testing/function1f.h>

namespace {
    using Function1fExternal = iqsm::binding::resource::Payload<base::testing::function1f>;

    auto evaluate_sample_resource(const iqsm::binding::resource::Data& data, float arg) -> float {
        const auto* function = dynamic_cast<const Function1fExternal*>(&data);
        if (!function) {
            throw std::runtime_error("SampleResource: unexpected runtime data type");
        }
        return function->value(arg);
    }
}

namespace Q1CORE::Etalon {
    auto SampleResource::Operations::use(Writing commit, Id id, resources::Manager manager, float arg) -> float {
        const Quantum& q = ops::particle::get<SampleResource>(commit.initial, id);
        ops::particle::modifier<SampleResource>(commit, id)->cost_summary += q.passport.cost;
        return evaluate_sample_resource(*manager->layer<SampleResource>()->get(id), arg);
    }

    auto SampleResource::Operations::use_free(Reading, Id id, resources::Manager manager, float arg) -> float {
        return evaluate_sample_resource(*manager->layer<SampleResource>()->get(id), arg);
    }

    const Invariants SampleResource::invariants{
        .structural = {},
        .logical = {},
    };

} // namespace Q1CORE::Etalon