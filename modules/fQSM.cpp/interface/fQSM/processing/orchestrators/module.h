#pragma once

#include <memory>
#include <utility>
#include <vector>

#include <base/maybe.h>
#include <fQSM/identifier.h>
#include <fQSM/manipulation/schema.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/intertype/schema.h>
#include <fQSM/processing/contexts/operational.h>

namespace fqsm::processing::orchestrator {

    using fqsm::Writing;

    // Module — shell: domain schema, children, installState → lived State.
    // State — life of the module in the world after install.
    class Module {
    public:
        virtual ~Module() = default;

        // Type-erased shared root id for inter-module awaken APIs.
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

            virtual void createDefaultState(Writing, RootId&) = 0;
            virtual void loadPastState(Writing) = 0;

            Schema fullSchema;
        };

        virtual Schema domain() = 0;
        virtual std::shared_ptr<State> installState(Schema finalSchema) = 0;

        Schema composedDomain();

        template<typename M, typename... Args>
        std::shared_ptr<M> add(Args&&... args);

        void add(std::shared_ptr<Module> child) { modules_.push_back(std::move(child)); }

        const std::vector<std::shared_ptr<Module>>& modules() const { return modules_; }

    protected:
        std::vector<std::shared_ptr<Module>> modules_;
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

    inline Schema Module::composedDomain() {
        Schema result = domain();
        for (auto& child : modules_)
            result = manipulation::schema::merge({result, child->composedDomain()});
        return result;
    }

    template<typename M, typename... Args>
    std::shared_ptr<M> Module::add(Args&&... args) {
        auto child = std::make_shared<M>(std::forward<Args>(args)...);
        modules_.push_back(child);
        return child;
    }

}
