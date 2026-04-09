#pragma once

#include <memory>
#include <utility>

#include <iQSM/_forwards.h>
#include <iQSM/meta/facade.h>
#include <iQSM/references.h>

namespace iqsm::binding {
    struct ManagerData;
}

namespace iqsm::binding::resource {
    struct Data {
        virtual ~Data() = default;
    };

    using Ptr = std::unique_ptr<Data>;
    using Provider = cref<::iqsm::binding::ManagerData>;
    using Manager = ref<::iqsm::binding::ManagerData>;

    template<typename T>
    struct Payload : Data {
        T value;

        explicit Payload(T value) : value(std::move(value)) {}
    };

    template<typename Meta>
    struct Loader {
        virtual ~Loader() = default;
        virtual Ptr load(World, Manager, Id<Meta>) const = 0;
        virtual void unload(World, Manager, Id<Meta>, Data&) const = 0;
    };

    template<typename TAspect, typename TPayload>
    struct Managed {
        using Aspect = TAspect;
        using Passport = typename Aspect::Passport;
        using Loader = iqsm::binding::resource::Loader<Aspect>;
        using Ptr = iqsm::binding::resource::Ptr;
        using PayloadType = TPayload;
        using Data = iqsm::binding::resource::Payload<PayloadType>;
    };
}