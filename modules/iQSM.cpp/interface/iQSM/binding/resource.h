#pragma once

#include <memory>
#include <utility>

namespace iqsm::binding::resource {
    struct Data {
        virtual ~Data() = default;
    };

    using Ptr = std::unique_ptr<Data>;

    template<typename T>
    struct Payload : Data {
        T value;

        explicit Payload(T value)
            : value(std::move(value)) {}
    };

    template<typename Meta>
    struct Loader {
        virtual ~Loader() = default;
        virtual Ptr load(const typename Meta::Passport&) const = 0;
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