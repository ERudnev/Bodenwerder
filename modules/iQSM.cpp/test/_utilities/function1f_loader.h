#pragma once

#include <cmath>
#include <memory>
#include <utility>

#include <base/testing/function1f.h>
#include <Etalon/resource.q1.h>

namespace tests::resources {

    using Function1f = iqsm::binding::resource::Managed<
        Q1CORE::Etalon::SampleResource,
        base::testing::function1f
    >;

    struct Function1fLoader final : Function1f::Loader {
        Function1f::Ptr load(iqsm::World world, iqsm::ref<iqsm::binding::ManagerData>, Function1f::Aspect::Id id) const override
        {
            const auto& passport = iqsm::helpers::particle::get<Function1f::Aspect>(world, id).passport;
            if (passport.description == "sin(x)") {
                return std::make_unique<Function1f::Data>(
                    Function1f::PayloadType{ [](float arg) { return std::sin(arg); } }
                );
            }
            if (passport.description == "cos(x)") {
                return std::make_unique<Function1f::Data>(
                    Function1f::PayloadType{ [](float arg) { return std::cos(arg); } }
                );
            }
            return {};
        }

        void unload(iqsm::World, iqsm::ref<iqsm::binding::ManagerData>, Function1f::Aspect::Id, iqsm::binding::resource::Data&) const override {}
    };
}

