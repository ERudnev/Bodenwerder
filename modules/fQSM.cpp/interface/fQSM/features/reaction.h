#pragma once

#include <stdexcept>
#include <type_traits>
#include <utility>

#include <fQSM/erased/links.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/processing/contexts/session.h>

namespace fqsm::manipulation {}

namespace fqsm::features::reactions {
    namespace ask = ::fqsm::manipulation;
}

namespace fqsm::features::reactions {

    // A reaction reads the changes of the aspects it listens to and writes corrections (see Reacting).
    struct Abstract {
        using Reacting = ::fqsm::Reacting;
        using Patch = model::complex::Patch;
        using Sources = meta::Rtid::Set;

        virtual ~Abstract() = default;

        virtual void apply(Reacting) = 0;
        virtual Sources listens() const = 0;
        // Links this reaction follows backwards (observed -> clients); the Realm keeps an inbound index for each.
        virtual std::vector<erased::LinkSpec> links() const { return {}; }

    protected:
        template<category::Any... Metas>
        static Sources typed_set() {
            return Sources{ TypeId<Metas>... };
        }

        template<category::Any Meta>
        static auto changes(const Reacting& context) -> ::fqsm::view::Delta<Meta> {
            return context.template changes<Meta>();
        }
    };

    // A reaction that calls one user function (a std::function or a function pointer).
    template<typename ActionFunctionType>
    struct Functional : reactions::Abstract {
        using ActionFunction = ActionFunctionType;

        explicit Functional(ActionFunction fn) : actionFunc(std::move(fn)) {}

    protected:
        template<typename... Args>
        auto action(Args&&... args) const -> std::invoke_result_t<const ActionFunction&, Args&&...> {
            if (not actionFunc) throw std::runtime_error("fQSM reaction: null function");
            return actionFunc(std::forward<Args>(args)...);
        }

    private:
        ActionFunction actionFunc = nullptr;
    };
}
