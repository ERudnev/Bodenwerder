#pragma once

#include <type_traits>
#include <stdexcept>
#include <utility>

#include <iQSM/_forwards.h>
#include <iQSM/delta.h>
#include <iQSM/field.h>
#include <iQSM/meta/aspect_id.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/meta/facade.h>
#include <iQSM/meta/global.h>
#include <iQSM/repository/transaction.h>

namespace iqsm::helpers::global {
    template<meta::Aspect Meta>
    auto get(World world) -> ::iqsm::meta::Global<Meta> { const auto field = world->field<Meta>(); return field->global; }

    template<meta::Aspect Meta>
    auto modifier(Writing writing);
}

namespace iqsm::detail::helpers::global {
    template<meta::Aspect Meta>
    class modifier final : protected repo::Transaction {
    public:
        using GlobalData = ::iqsm::meta::GlobalData<Meta>;
        using Global = ::iqsm::meta::Global<Meta>;

        explicit modifier(Writing writing)
            : Transaction(std::move(writing))
            , original(required_item(head.state))
            , value(*original)
        {}

        ~modifier() override { on_finish(); }
        modifier(const modifier&) = delete;
        modifier& operator=(const modifier&) = delete;
        modifier(modifier&&) = delete;
        modifier& operator=(modifier&&) = delete;

        GlobalData* operator->() { dirty = true; return &value; }
        GlobalData& operator*() { dirty = true; return value; }

        void on_finish() override {
            if (unwinding()) return;
            if (finished) return;
            finished = true;
            if (not head.upstream) return;
            if (!dirty) { disconnect(); return; }

            auto field_delta = base::make_shared<delta::FieldDiff<Meta>>();
            field_delta->global_change = std::pair<typename delta::FieldDiff<Meta>::Global, typename delta::FieldDiff<Meta>::Global>{
                original,
                base::make_shared<const GlobalData>(std::move(value)),
            };

            auto world_delta = base::make_shared<delta::Fields>();
            world_delta->fields.emplace(types::aspectId<Meta>(), freeze(field_delta));

            head.upstream(freeze(world_delta));
            disconnect();
        }

    private:
        static Global required_item(World world) {
            if (not world->schema->aspects.contains(types::aspectId<Meta>())) {
                throw std::runtime_error("helpers::global::modifier(): aspect is not in schema");
            }
            return world->field<Meta>()->global;
        }

    protected:
        void absorb(Delta delta) override {}

        Global original;
        GlobalData value;
        bool dirty = false;
        bool finished = false;
    };
}

namespace iqsm::helpers::global {
    template<meta::Aspect Meta>
    auto modifier(Writing writing) {
        return detail::helpers::global::modifier<Meta>(writing);
    }
}

