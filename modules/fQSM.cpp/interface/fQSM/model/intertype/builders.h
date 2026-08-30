#pragma once

#include <concepts>
#include <type_traits>

#include <base/shared_reference.h>

#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/complex/reality.h>
#include <fQSM/model/linear/future.h>
#include <fQSM/model/linear/patch.h>
#include <fQSM/model/linear/reality.h>
#include <fQSM/model/intertype/binding.h>
#include <fQSM/processing/algorithms/integration.h>
#include <fQSM/processing/algorithms/merge.h>
#include <fQSM/processing/contexts/settingUp.h>
#include <fQSM/utility/logging.h>

namespace fqsm::schema::details {

    template<typename Meta>
    concept HasGlobalAssemble = requires (fqsm::SettingUp& setup) {
        { Meta::Always::assemble(setup) } -> std::same_as<GlobalValue<Meta>>;
    };

    template<category::Any Meta>
    auto createFuture(const model::complex::State& state, ref<model::complex::Patch> patch) -> ref<model::linear::state::Erased> {
        auto castedPatch = base::shared_ref_cast<model::linear::Patch<Meta>>(patch->lines.container.at(TypeId<Meta>));
        return base::make_shared<model::linear::Future<Meta>>(state.aspect<Meta>(), castedPatch);
    }

    template<category::Any Meta>
    void integratePatchSlice(model::complex::Reality& world, const model::complex::Patch& patch) {
        fqsm::processing::algorithm::details::integrate<Meta>(world, patch);
    }

    template<category::Any Meta>
    void mergePatchSlice(const model::complex::State& base, model::complex::Patch& target, const model::complex::Patch& source) {
        fqsm::processing::algorithm::details::merge<Meta>(base, target, source);
    }

    template<category::Any Meta>
    auto binding() -> model::intertype::Binding {
        model::intertype::Binding out{
            .patch = {
                .create = &model::linear::Patch<Meta>::create,
                .absorb = &model::complex::Patch::absorb<Meta>,
                .clear = &model::complex::Patch::clear<Meta>,
                .log = &utility::detail::log_patch_slice<Meta>,
            },
            .state = {
                .create = {},
                .clone = &model::complex::Reality::clone<Meta>,
            },
            .createFuture = &createFuture<Meta>,
            .integratePatchSlice = &integratePatchSlice<Meta>,
            .mergePatchSlice = &mergePatchSlice<Meta>,
            .assemble = {},
        };
        if constexpr (HasGlobalAssemble<Meta>) {
            out.assemble = [](fqsm::SettingUp& setup) {
                auto global = Meta::Always::assemble(setup);
                setup.emplace(TypeId<Meta>, model::linear::Reality<Meta>::createWith(global));
            };
        } else {
            static_assert(std::is_default_constructible_v<GlobalValue<Meta>>, "fQSM: Global is not default-constructible; declare always >assemble() -> all");
            out.state.create = &model::linear::Reality<Meta>::create;
        }
        return out;
    }
}
