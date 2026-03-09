#pragma once

#include <format>
#include <stdexcept>
#include <utility>

#include <iQSM/_forwards.h>
#include <iQSM/meta.h>
#include <iQSM/delta.h>
#include <iQSM/field.h>
#include <iQSM/operations/transaction.h>

namespace iqsm::ops::particle {
  template<meta::Particle Meta>
  auto modify(Transaction& transaction, typename Facet<Meta>::Id id);

  template<meta::Entity Meta>
  auto create(Transaction& transaction);

  template<meta::Quark Meta>
  auto create(Transaction& transaction, typename Facet<Meta>::Id id);

  template<meta::Particle Meta>
  void remove(Transaction& transaction, typename Facet<Meta>::Id id);

  template<meta::Particle Meta>
  auto get(World world, typename Facet<Meta>::Id id) -> const typename Facet<Meta>::Quantum&;

  template<meta::Particle Meta>
  bool exists(World world, typename Facet<Meta>::Id id);

} // namespace iqsm::ops::particle

namespace iqsm::detail::ops::particle {
    using ::iqsm::ops::Transaction;

    template<meta::Particle Meta>
    class modifier {
    public:
      using Id = typename Facet<Meta>::Id;
      using Quantum = typename Facet<Meta>::Quantum;
      using Item = typename Facet<Meta>::Item;

      modifier(Transaction& transaction, Id id)
          : transaction(transaction)
          , id(id)
          , original(required_item(transaction.current, id))
          , value(*original)
          , dirty(true)
      {}
      ~modifier() { apply(); }
      modifier(const modifier&) = delete;
      modifier& operator=(const modifier&) = delete;
      modifier(modifier&& other) noexcept : transaction(other.transaction), id(other.id), original(other.original), value(std::move(other.value)), dirty(other.dirty), applied(other.applied) { other.applied = true; }
      modifier& operator=(modifier&&) = delete;
      Quantum* operator->() { dirty = true; return &value; }
      Quantum& operator*() { dirty = true; return value; }

    private:
      static Item required_item(World world, const Id& id);
      void apply();

      Transaction& transaction;
      Id id;
      Item original;
      Quantum value;
      bool dirty = false;
      bool applied = false;
    };

    template<meta::Entity Meta>
    class creator_entity {
    public:
      using Id = typename Facet<Meta>::Id;
      using Quantum = typename Facet<Meta>::Quantum;

      explicit creator_entity(Transaction& transaction) : transaction(transaction) {}
      Id operator()(Quantum value);

    private:
      Transaction& transaction;
    };

    template<meta::Quark Meta>
    class creator_quark {
    public:
      using Id = typename Facet<Meta>::Id;
      using Quantum = typename Facet<Meta>::Quantum;

      creator_quark(Transaction& transaction, Id id) : transaction(transaction), id(id) {}
      Id operator()(Quantum value);

    private:
      Transaction& transaction;
      Id id;
    };
} // namespace iqsm::detail::ops::particle

namespace iqsm::ops::particle {
  template<meta::Particle Meta>
  auto get(World world, typename Facet<Meta>::Id id) -> const typename Facet<Meta>::Quantum& {
    const auto field = world->field<Meta>();
    if (not field->container.contains(id)) { throw std::runtime_error(std::format("ops::particle::get(): missing entity: {}", id)); }
    const auto item = field->container.at(id);
    return *item;
  }

  template<meta::Particle Meta>
  bool exists(World world, typename Facet<Meta>::Id id) {
    return world->field<Meta>()->container.contains(id);
  }
} // namespace iqsm::ops::particle

namespace iqsm::ops::particle {
  template<meta::Particle Meta>
  auto modify(Transaction& transaction, typename Facet<Meta>::Id id) { return detail::ops::particle::modifier<Meta>(transaction, id); }

  template<meta::Entity Meta>
  auto create(Transaction& transaction) { return detail::ops::particle::creator_entity<Meta>(transaction); }

  template<meta::Quark Meta>
  auto create(Transaction& transaction, typename Facet<Meta>::Id id) { return detail::ops::particle::creator_quark<Meta>(transaction, id); }

  template<meta::Particle Meta>
  void remove(Transaction& transaction, typename Facet<Meta>::Id id) {
    auto fd = base::make_shared<delta::FieldDiff<Meta>>();
    auto op = typename delta::FieldDiff<Meta>::Operation{};
    op.remove = true;
    fd->ops = fd->ops.insert(id, std::move(op));

    auto wd = base::make_shared<delta::Fields>();
    wd->fields = wd->fields.insert(
        Facet<Meta>::typeId,
        freeze(fd));

    transaction.absorb(freeze(wd));
  }

} // namespace iqsm::ops::particle

namespace iqsm::detail::ops::particle {
  template<meta::Particle Meta>
  typename modifier<Meta>::Item modifier<Meta>::required_item(World world, const Id& id) {
    const auto field = world->field<Meta>();
    if (not field->container.contains(id)) { throw std::runtime_error(std::format("modifier: missing entity: {}", id)); }
    const auto item = field->container.at(id);
    return item;
  }

  template<meta::Particle Meta>
  void modifier<Meta>::apply() {
    if (applied) return;
    applied = true;
    if (!dirty) return;

    auto fd = base::make_shared<delta::FieldDiff<Meta>>();
    auto op = typename delta::FieldDiff<Meta>::Operation{};
    op.change = std::pair<typename delta::FieldDiff<Meta>::Item, typename delta::FieldDiff<Meta>::Item>{
        original,
        Facet<Meta>::create(std::move(value)),
    };
    fd->ops = fd->ops.insert(id, std::move(op));

    auto wd = base::make_shared<delta::Fields>();
    wd->fields = wd->fields.insert(
        Facet<Meta>::typeId,
        freeze(fd));

    transaction.absorb(freeze(wd));
  }

  template<meta::Entity Meta>
  typename creator_entity<Meta>::Id creator_entity<Meta>::operator()(Quantum value) {
    const auto id = Id::generate_random();

    auto fd = base::make_shared<delta::FieldDiff<Meta>>();
    auto op = typename delta::FieldDiff<Meta>::Operation{};
    op.add = Facet<Meta>::create(std::move(value));
    fd->ops = fd->ops.insert(id, std::move(op));

    auto wd = base::make_shared<delta::Fields>();
    wd->fields = wd->fields.insert(
        Facet<Meta>::typeId,
        freeze(fd));

    transaction.absorb(freeze(wd));
    return id;
  }

  template<meta::Quark Meta>
  typename creator_quark<Meta>::Id creator_quark<Meta>::operator()(Quantum value) {
    auto fd = base::make_shared<delta::FieldDiff<Meta>>();
    auto op = typename delta::FieldDiff<Meta>::Operation{};
    op.add = Facet<Meta>::create(std::move(value));
    fd->ops = fd->ops.insert(id, std::move(op));

    auto wd = base::make_shared<delta::Fields>();
    wd->fields = wd->fields.insert(
        Facet<Meta>::typeId,
        freeze(fd));

    transaction.absorb(freeze(wd));
    return id;
  }
} // namespace iqsm::detail::ops::particle

