#pragma once

#include <type_traits>
#include <utility>

#include <iQSM/_forwards.h>
#include <iQSM/meta.h>
#include <iQSM/delta.h>
#include <iQSM/field.h>
#include <iQSM/operations/transaction.h>

namespace iqsm::ops::global {
    template<meta::Aspect Meta>
    auto get(World world) -> typename Facet<Meta>::Global {
        const auto field = world->field<Meta>();
        return field->global;
    }

    template<meta::Aspect Meta>
    auto modify(Transaction& transaction);
}

namespace iqsm::detail::ops::global {
    using ::iqsm::ops::Transaction;

    template<meta::Aspect Meta>
    class modifier {
    public:
        using GlobalData = typename Facet<Meta>::GlobalData;
        using Global = typename Facet<Meta>::Global;

        explicit modifier(Transaction& transaction)
            : transaction(transaction)
            , original(required_item(transaction.current))
            , value(*original)
        {}

        ~modifier() { apply(); }
        modifier(const modifier&) = delete;
        modifier& operator=(const modifier&) = delete;
        modifier(modifier&& other) noexcept
            : transaction(other.transaction)
            , original(other.original)
            , value(std::move(other.value))
            , dirty(other.dirty)
            , applied(other.applied)
        { other.applied = true; }
        modifier& operator=(modifier&&) = delete;

        GlobalData* operator->() { dirty = true; return &value; }
        GlobalData& operator*() { dirty = true; return value; }

    private:
        static Global required_item(World world) {
            if (not world->schema->aspects.contains(Facet<Meta>::typeId)) {
                throw std::runtime_error("ops::global::modify(): aspect is not in schema");
            }
            return world->field<Meta>()->global;
        }

        void apply() {
            if (applied) return;
            applied = true;
            if (!dirty) return;

            auto fd = base::make_shared<delta::FieldDiff<Meta>>();
            fd->global_change = std::pair<typename delta::FieldDiff<Meta>::Global, typename delta::FieldDiff<Meta>::Global>{
                original,
                base::make_shared<const GlobalData>(std::move(value)),
            };

            auto wd = base::make_shared<delta::Fields>();
            wd->fields = wd->fields.insert(
                Facet<Meta>::typeId,
                freeze(fd));

            transaction.absorb(freeze(wd));
        }

        Transaction& transaction;
        Global original;
        GlobalData value;
        bool dirty = false;
        bool applied = false;
    };
}

namespace iqsm::ops::global {
    template<meta::Aspect Meta>
    auto modify(Transaction& transaction) {
        return detail::ops::global::modifier<Meta>(transaction);
    }
}

