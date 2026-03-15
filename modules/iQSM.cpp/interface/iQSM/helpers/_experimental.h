#pragma once

#include <format>
#include <stdexcept>
#include <utility>

#include <iQSM/delta.h>
#include <iQSM/field.h>
#include <iQSM/meta.h>
#include <iQSM/repository/commit.h>

namespace iqsm::detail::ops::experimental {
    template<meta::Particle Meta>
    class modifier {
    public:
        using Id = iqsm::Id<Meta>;
        using Item = typename Facet<Meta>::Item;
        using Quantum = iqsm::Quantum<Meta>;

    private:
        repo::Commit commit;
        Id id;
        Item original;
        Quantum value;
        bool dirty = false;
        bool applied = false;

    public:
        modifier(repo::Commit commit, Id id);
        ~modifier();
        modifier(const modifier&) = delete;
        modifier& operator=(const modifier&) = delete;
        modifier(modifier&& other) noexcept;
        modifier& operator=(modifier&&) = delete;

        Quantum* operator->();
        Quantum& operator*();

    private:
        static Item required_item(World world, const Id& id);
        void apply();
    };

}

namespace iqsm::ops::experimental::particle {
    template<meta::Entity Meta>
    Id<Meta> create(repo::Commit commit, Quantum<Meta> value) {
        const auto id = Id<Meta>::generate_random();

        auto field_delta = base::make_shared<delta::FieldDiff<Meta>>();
        auto operation = typename delta::FieldDiff<Meta>::Operation{};
        operation.add = Facet<Meta>::create(std::move(value));
        field_delta->ops = field_delta->ops.insert(id, std::move(operation));

        auto world_delta = base::make_shared<delta::Fields>();
        world_delta->fields = world_delta->fields.insert(Facet<Meta>::typeId, freeze(field_delta));

        commit.push(freeze(world_delta));
        return id;
    }

    template<meta::Particle Meta>
    auto modifier(repo::Commit commit, Id<Meta> id) {
        return detail::ops::experimental::modifier<Meta>(std::move(commit), id);
    }

    template<meta::Particle Meta>
    void remove(repo::Commit commit, Id<Meta> id) {
        auto field_delta = base::make_shared<delta::FieldDiff<Meta>>();
        auto operation = typename delta::FieldDiff<Meta>::Operation{};
        operation.remove = true;
        field_delta->ops = field_delta->ops.insert(id, std::move(operation));

        auto world_delta = base::make_shared<delta::Fields>();
        world_delta->fields = world_delta->fields.insert(Facet<Meta>::typeId, freeze(field_delta));

        commit.push(freeze(world_delta));
    }
}

namespace iqsm::detail::ops::experimental {
    template<meta::Particle Meta>
    modifier<Meta>::modifier(repo::Commit commit, Id id)
        : commit(std::move(commit))
        , id(id)
        , original(required_item(this->commit.initial, id))
        , value(*original)
    {}

    template<meta::Particle Meta>
    modifier<Meta>::~modifier() {
        apply();
    }

    template<meta::Particle Meta>
    modifier<Meta>::modifier(modifier&& other) noexcept
        : commit(std::move(other.commit))
        , id(other.id)
        , original(other.original)
        , value(std::move(other.value))
        , dirty(other.dirty)
        , applied(other.applied)
    {
        other.applied = true;
    }

    template<meta::Particle Meta>
    typename modifier<Meta>::Quantum* modifier<Meta>::operator->() {
        dirty = true;
        return &value;
    }

    template<meta::Particle Meta>
    typename modifier<Meta>::Quantum& modifier<Meta>::operator*() {
        dirty = true;
        return value;
    }

    template<meta::Particle Meta>
    typename modifier<Meta>::Item modifier<Meta>::required_item(World world, const Id& id) {
        const auto field = world->field<Meta>();
        if (not field->container.contains(id)) {
            throw std::runtime_error(std::format("modifier(repo): missing entity: {}", id));
        }
        return field->container.at(id);
    }

    template<meta::Particle Meta>
    void modifier<Meta>::apply() {
        if (applied) return;
        applied = true;
        if (!dirty) return;

        auto field_delta = base::make_shared<delta::FieldDiff<Meta>>();
        auto operation = typename delta::FieldDiff<Meta>::Operation{};
        operation.change = std::pair<typename delta::FieldDiff<Meta>::Item, typename delta::FieldDiff<Meta>::Item>{original, Facet<Meta>::create(std::move(value))};
        field_delta->ops = field_delta->ops.insert(id, std::move(operation));

        auto world_delta = base::make_shared<delta::Fields>();
        world_delta->fields = world_delta->fields.insert(Facet<Meta>::typeId, freeze(field_delta));

        commit.push(freeze(world_delta));
    }

}