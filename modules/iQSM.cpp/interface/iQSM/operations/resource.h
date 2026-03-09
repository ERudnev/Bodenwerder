#pragma once

#include <optional>
#include <stdexcept>

#include <iQSM/meta.h>
#include <iQSM/_forwards.h>
#include <iQSM/delta.h>
#include <iQSM/operations/transaction.h>

namespace iqsm::ops::resource {
    template<meta::Resource Meta>
    auto declare(Transaction& transaction);

    template<meta::Resource Meta>
    auto get(World world, typename Facet<Meta>::Id id) -> const typename Facet<Meta>::Quantum&;

    template<meta::Resource Meta>
    bool exists(World world, typename Facet<Meta>::Id id);
}

namespace iqsm::detail::ops::resource {
    using ::iqsm::ops::Transaction;

    template<meta::Resource Meta>
    class declarer {
    public:
        using Id = typename Facet<Meta>::Id;
        using Quantum = typename Facet<Meta>::Quantum;

        explicit declarer(Transaction& tx) : transaction(tx) {}

        Id operator()(Quantum value) {
            const auto id = Id::generate_random();

            auto fd = base::make_shared<delta::FieldDiff<Meta>>();
            auto op = typename delta::FieldDiff<Meta>::Operation{};
            op.add = Facet<Meta>::create(std::move(value));
            fd->ops = fd->ops.insert(id, std::move(op));

            auto wd = base::make_shared<delta::Fields>();
            wd->fields = wd->fields.insert(
                Facet<Meta>::typeId,
                freeze(fd));

            this->transaction.absorb(freeze(wd));
            return id;
        }

    private:
        Transaction& transaction;
    };
}

namespace iqsm::ops::resource {
    template<meta::Resource Meta>
    auto declare(Transaction& transaction) {
        return detail::ops::resource::declarer<Meta>(transaction);
    }

    template<meta::Resource Meta>
    auto get(World world, typename Facet<Meta>::Id id) -> const typename Facet<Meta>::Quantum& {
        const auto field = world->field<Meta>();
        if (not field->container.contains(id)) { throw std::runtime_error(std::format("ops::resource::get(): missing instance: {}", id)); }
        const auto item = field->container.at(id);
        return *item;
    }

    template<meta::Resource Meta>
    bool exists(World world, typename Facet<Meta>::Id id) {
        return world->field<Meta>()->container.contains(id);
    }
}

