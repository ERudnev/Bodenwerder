#pragma once

#include <memory>
#include <utility>
#include <vector>

#include <fQSM/model/_forwards.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::processing::orchestrator {

    // Module: a unit of installation. schema() gives this module's aspects plus the submodules' schemas;
    // install() turns the final schema into the lived State of the module.
    class Module {
    public:
        virtual ~Module() = default;

        struct State {
            explicit State(Schema schema) : fullSchema(std::move(schema)) {}
            virtual ~State() = default;

            virtual void loadPastState(Writing) = 0;

            Schema fullSchema;
        };

        virtual Schema schema() = 0;
        virtual std::shared_ptr<State> install(Schema) = 0;

        template<typename M, typename... Args>
        std::shared_ptr<M> add(Args&&... args) {
            auto child = std::make_shared<M>(std::forward<Args>(args)...);
            submodules.push_back(child);
            return child;
        }

        void add(std::shared_ptr<Module> child) { submodules.push_back(std::move(child)); }

    protected:
        std::vector<std::shared_ptr<Module>> submodules;
    };
}
