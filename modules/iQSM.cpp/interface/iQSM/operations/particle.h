#pragma once

#include <format>
#include <optional>
#include <stdexcept>
#include <utility>

#include <iQSM/_forwards.h>
#include <iQSM/aspects.h>
#include <iQSM/delta.h>
#include <iQSM/field.h>
#include <iQSM/operations/transaction.h>

namespace iqsm::ops::particle {
  template<AspectParticle Meta>
  auto modify(Transaction& transaction, typename Facet<Meta>::Id id);

  template<AspectParticle Meta>
  auto create(Transaction& transaction) requires AspectXion<Meta>;

  template<AspectParticle Meta>
  auto create(Transaction& transaction, typename Facet<Meta>::Id id) requires (!AspectXion<Meta>);

  template<AspectParticle Meta>
  void remove(Transaction& transaction, typename Facet<Meta>::Id id);

  template<AspectParticle Meta>
  auto get(World world, typename Facet<Meta>::Id id) -> typename Facet<Meta>::Item;

  template<AspectParticle Meta>
  auto get_opt(World world, typename Facet<Meta>::Id id) -> std::optional<typename Facet<Meta>::Item>;

  template<AspectParticle Meta>
  bool contains(World world, typename Facet<Meta>::Id id);

} // namespace iqsm::ops::particle

namespace iqsm::detail::ops::particle {
    using ::iqsm::ops::Transaction;

    template<AspectParticle Meta>
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

    template<AspectParticle Meta>
    class creator_xion {
    public:
      using Id = typename Facet<Meta>::Id;
      using Quantum = typename Facet<Meta>::Quantum;

      explicit creator_xion(Transaction& transaction) : transaction(transaction) {}
      Id operator()(Quantum value);

    private:
      Transaction& transaction;
    };

    template<AspectParticle Meta>
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
  template<AspectParticle Meta>
  auto get(World world, typename Facet<Meta>::Id id) -> typename Facet<Meta>::Item {
    required(world, "ops::particle::get(): world");
    const auto field = world->field<Meta>();
    if (not field->container.contains(id)) { throw std::runtime_error(std::format("ops::particle::get(): missing entity: {}", id)); }
    const auto item = field->container.at(id);
    required(item, "ops::particle::get(): item");
    return item;
  }

  template<AspectParticle Meta>
  auto get_opt(World world, typename Facet<Meta>::Id id) -> std::optional<typename Facet<Meta>::Item> {
    required(world, "ops::particle::get_opt(): world");
    const auto field = world->field<Meta>();
    if (not field->container.contains(id)) { return std::nullopt; }
    return field->container.at(id);
  }

  template<AspectParticle Meta>
  bool contains(World world, typename Facet<Meta>::Id id) {
    required(world, "ops::particle::contains(): world");
    return world->field<Meta>()->container.contains(id);
  }
} // namespace iqsm::ops::particle

namespace iqsm::ops::particle {
  template<AspectParticle Meta>
  auto modify(Transaction& transaction, typename Facet<Meta>::Id id) { return detail::ops::particle::modifier<Meta>(transaction, id); }

  template<AspectParticle Meta>
  auto create(Transaction& transaction) requires AspectXion<Meta> { return detail::ops::particle::creator_xion<Meta>(transaction); }

  template<AspectParticle Meta>
  auto create(Transaction& transaction, typename Facet<Meta>::Id id) requires (!AspectXion<Meta>) { return detail::ops::particle::creator_quark<Meta>(transaction, id); }

  template<AspectParticle Meta>
  void remove(Transaction& transaction, typename Facet<Meta>::Id id) {
    auto fd = std::make_shared<delta::FieldDiff<Meta>>();
    fd->deleted = fd->deleted.insert(id);

    auto wd = std::make_shared<delta::WorldState>();
    wd->fields = wd->fields.insert(
        Facet<Meta>::typeId,
        std::static_pointer_cast<const delta::FieldDiffAbstract>(freeze(fd)));

    transaction.absorb(freeze(wd));
  }

} // namespace iqsm::ops::particle

namespace iqsm::detail::ops::particle {
  template<AspectParticle Meta>
  typename modifier<Meta>::Item modifier<Meta>::required_item(World world, const Id& id) {
    required(world, "modifier: world");
    const auto field = world->field<Meta>();
    if (not field->container.contains(id)) { throw std::runtime_error(std::format("modifier: missing entity: {}", id)); }
    const auto item = field->container.at(id);
    required(item, "modifier: item");
    return item;
  }

  template<AspectParticle Meta>
  void modifier<Meta>::apply() {
    if (applied) return;
    applied = true;
    if (!dirty) return;

    auto fd = std::make_shared<delta::FieldDiff<Meta>>();
    fd->changed = fd->changed.insert(id, typename delta::FieldDiff<Meta>::Change{
        .before = original,
        .after = Facet<Meta>::create(std::move(value)),
    });

    auto wd = std::make_shared<delta::WorldState>();
    wd->fields = wd->fields.insert(
        Facet<Meta>::typeId,
        std::static_pointer_cast<const delta::FieldDiffAbstract>(freeze(fd)));

    transaction.absorb(freeze(wd));
  }

  template<AspectParticle Meta>
  typename creator_xion<Meta>::Id creator_xion<Meta>::operator()(Quantum value) {
    const auto id = Id::generate_random();

    auto fd = std::make_shared<delta::FieldDiff<Meta>>();
    fd->added = fd->added.insert(id, Facet<Meta>::create(std::move(value)));

    auto wd = std::make_shared<delta::WorldState>();
    wd->fields = wd->fields.insert(
        Facet<Meta>::typeId,
        std::static_pointer_cast<const delta::FieldDiffAbstract>(freeze(fd)));

    transaction.absorb(freeze(wd));
    return id;
  }

  template<AspectParticle Meta>
  typename creator_quark<Meta>::Id creator_quark<Meta>::operator()(Quantum value) {
    auto fd = std::make_shared<delta::FieldDiff<Meta>>();
    fd->added = fd->added.insert(id, Facet<Meta>::create(std::move(value)));

    auto wd = std::make_shared<delta::WorldState>();
    wd->fields = wd->fields.insert(
        Facet<Meta>::typeId,
        std::static_pointer_cast<const delta::FieldDiffAbstract>(freeze(fd)));

    transaction.absorb(freeze(wd));
    return id;
  }
} // namespace iqsm::detail::ops::particle

