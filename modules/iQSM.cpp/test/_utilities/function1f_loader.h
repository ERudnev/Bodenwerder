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
        Function1f::Ptr load(const Function1f::Passport& passport) const override
        {
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
    };
}

