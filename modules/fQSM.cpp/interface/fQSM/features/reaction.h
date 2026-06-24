#pragma once

#include <vector>

#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/processing/review.h>
#include <fQSM/manipulation/feedback.h>

namespace fqsm::manipulation {}

namespace fqsm::features::reactions {
    namespace ask = ::fqsm::manipulation;
}

namespace fqsm::features::reactions {

    // Norma is a special kind of Reaction; Behavior collects rules specifically.
    // TODO: consider as template<ActionType>,
    struct Abstract {
        using Reviewing = processing::Review;
        using Draft = model::complex::Draft;
        using Patch = model::complex::Patch;
        using Sources = meta::Rtid::Set;

        virtual ~Abstract() = default;

        virtual void apply(Reviewing) = 0;
        virtual Sources listens() const = 0;

    protected:
        // derived class helper:
        template<category::Any... Metas>
        static Sources typed_set() {
            return Sources{ TypeId<Metas>... };
        }

        template<category::Any Meta>
        static auto changes(const Reviewing& context) -> model::linear::Delta<Meta> {
            return context.template changes<Meta>();
        }
    };

    template<typename ActionFunctionType>
    struct Functional : reactions::Abstract {
        using ActionFunction = ActionFunctionType;
        using reactions::Abstract::Reviewing;
        using reactions::Abstract::Draft;
        using reactions::Abstract::Patch;
        using reactions::Abstract::Sources;

        explicit Functional(ActionFunction fn) : actionFunc(fn) {}
    protected:

        // Review first: Reviewing -> Writing for Action/ConstructFromParent signatures.
        template<typename... Rest>
        auto action(Reviewing reviewing, Rest&&... rest) const
            -> std::invoke_result_t<ActionFunction, Reviewing, Rest&&...>
        {
            return invoke_action(::fqsm::Writing{reviewing}, std::forward<Rest>(rest)...);
        }

        // Direct forward (QuantumLocal, explicit Writing, etc.).
        template<typename First, typename... Rest>
        auto action(First&& first, Rest&&... rest) const
            -> std::invoke_result_t<ActionFunction, First&&, Rest&&...>
            requires (
                !std::convertible_to<std::remove_cvref_t<First>, Reviewing> ||
                std::same_as<std::remove_cvref_t<First>, ::fqsm::Writing>
            )
        {
            return invoke_action(std::forward<First>(first), std::forward<Rest>(rest)...);
        }

        bool optionally_callable(Reviewing context, std::string_view reason) const {
            if (actionFunc) return true;
            context.notes.critical.push_back(std::string{reason});
            return false;
        }

    private:
        template<typename... Args>
        auto invoke_action(Args&&... args) const -> std::invoke_result_t<ActionFunction, Args&&...> {
            if (!actionFunc) throw std::runtime_error("null func");
            if constexpr (std::is_void_v<std::invoke_result_t<ActionFunction, Args&&...>>) {
                actionFunc(std::forward<Args>(args)...);
            } else {
                return actionFunc(std::forward<Args>(args)...);
            }
        }

        ActionFunction actionFunc = nullptr;
    };
}
