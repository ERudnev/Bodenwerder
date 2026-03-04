#pragma once

#include <optional>
#include <stdexcept>

#include <iQSM/aspects.h>
#include <iQSM/_forwards.h>
#include <iQSM/delta.h>
#include <iQSM/operations/transaction.h>

namespace iqsm::ops::resource {
    template<AspectResource Meta>
    auto declare(Transaction& transaction);

    template<AspectResource Meta>
    auto get(World world, typename Facet<Meta>::Id id) -> typename Facet<Meta>::Item;

    template<AspectResource Meta>
    auto get_opt(World world, typename Facet<Meta>::Id id) -> std::optional<typename Facet<Meta>::Item>;

    template<AspectResource Meta>
    bool contains(World world, typename Facet<Meta>::Id id);
}

namespace iqsm::detail::ops::resource {
    using ::iqsm::ops::Transaction;

    template<AspectResource Meta>
    class declarer {
    public:
        using Id = typename Facet<Meta>::Id;
        using Quantum = typename Facet<Meta>::Quantum;

        explicit declarer(Transaction& tx) : transaction(tx) {}

        Id operator()(Quantum value) {
            const auto id = Id::generate_random();

            auto fd = std::make_shared<delta::FieldDiff<Meta>>();
            fd->added = fd->added.insert(id, Facet<Meta>::create(std::move(value)));

            auto wd = std::make_shared<delta::WorldState>();
            wd->fields = wd->fields.insert(
                Facet<Meta>::typeId,
                std::static_pointer_cast<const delta::FieldDiffAbstract>(freeze(fd)));

            this->transaction.absorb(freeze(wd));
            return id;
        }

    private:
        Transaction& transaction;
    };
}

namespace iqsm::ops::resource {
    template<AspectResource Meta>
    auto declare(Transaction& transaction) {
        return detail::ops::resource::declarer<Meta>(transaction);
    }

    template<AspectResource Meta>
    auto get(World world, typename Facet<Meta>::Id id) -> typename Facet<Meta>::Item {
        required(world, "ops::resource::get(): world");
        const auto field = world->field<Meta>();
        if (not field->container.contains(id)) { throw std::runtime_error(std::format("ops::resource::get(): missing instance: {}", id)); }
        const auto item = field->container.at(id);
        required(item, "ops::resource::get(): item");
        return item;
    }

    template<AspectResource Meta>
    auto get_opt(World world, typename Facet<Meta>::Id id) -> std::optional<typename Facet<Meta>::Item> {
        required(world, "ops::resource::get_opt(): world");
        const auto field = world->field<Meta>();
        if (not field->container.contains(id)) { return std::nullopt; }
        return field->container.at(id);
    }

    template<AspectResource Meta>
    bool contains(World world, typename Facet<Meta>::Id id) {
        required(world, "ops::resource::contains(): world");
        return world->field<Meta>()->container.contains(id);
    }
}

