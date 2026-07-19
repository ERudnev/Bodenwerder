#pragma once

#include <memory>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/maybe.h>
#include <fQSM/identifier.h>
#include <fQSM/manipulation/schema.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/intertype/schema.h>
#include <fQSM/processing/contexts/operational.h>

namespace fqsm::processing::orchestrator {

    using fqsm::Writing;

    // Module — shell: submodules, installState → lived State.
    // schema() — this module's types plus submodules' schema(), recursively.
    // State — life of the module in the world after install.
    class Module {
    public:
        virtual ~Module() = default;

        // Type-erased shared root id for inter-module start APIs.
        // Not a general id hack: only Module protocol; family secrets via secretGet/secretSet.
        struct RootId {
            template<meta::category::Any Meta>
            base::maybe<Identifier<Meta>> secretGet() const;

            template<meta::category::Any Meta>
            void secretSet(Identifier<Meta> value);

        private:
            Identifier<RootId>::Raw raw{};
            base::maybe<meta::Rtid> actualTypeId;
        };

        struct State {
            explicit State(Schema schema) : fullSchema(std::move(schema)) {}
            virtual ~State() = default;

            virtual void loadPastState(Writing) = 0;

            Schema fullSchema;
        };

        virtual Schema schema() = 0;

        // Default: unused on roots that install via a typed API. Submodules override.
        virtual std::shared_ptr<State> installState(Schema) { _INCOMPLETE_; }

        template<typename M, typename... Args>
        std::shared_ptr<M> add(Args&&... args);

        void add(std::shared_ptr<Module> child) { submodules.push_back(std::move(child)); }

    protected:
        std::vector<std::shared_ptr<Module>> submodules;
    };

}

namespace fqsm::processing::orchestrator {

    template<meta::category::Any Meta>
    base::maybe<Identifier<Meta>> Module::RootId::secretGet() const {
        if (not actualTypeId.exists())
            return {};
        if (static_cast<const meta::Rtid&>(actualTypeId) != meta::Rtid::of<Meta>())
            return {};
        return Identifier<Meta>{raw};
    }

    template<meta::category::Any Meta>
    void Module::RootId::secretSet(Identifier<Meta> value) {
        raw = value.raw();
        actualTypeId = meta::Rtid::of<Meta>();
    }

    template<typename M, typename... Args>
    std::shared_ptr<M> Module::add(Args&&... args) {
        auto child = std::make_shared<M>(std::forward<Args>(args)...);
        submodules.push_back(child);
        return child;
    }

}
