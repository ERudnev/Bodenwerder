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
  template<Facet Meta>
  auto modify(Transaction& transaction, typename Aspect<Meta>::Id id);

  template<Facet Meta>
  auto create(Transaction& transaction) requires XionFacet<Meta>;

  template<Facet Meta>
  auto create(Transaction& transaction, typename Aspect<Meta>::Id id) requires (!XionFacet<Meta>);

  template<Facet Meta>
  void remove(Transaction& transaction, typename Aspect<Meta>::Id id);

  template<Facet Meta>
  auto get(World world, typename Aspect<Meta>::Id id) -> typename Aspect<Meta>::Item;

  template<Facet Meta>
  auto get_opt(World world, typename Aspect<Meta>::Id id) -> std::optional<typename Aspect<Meta>::Item>;

  template<Facet Meta>
  bool contains(World world, typename Aspect<Meta>::Id id);

} // namespace iqsm::ops::particle

namespace iqsm::detail::ops::particle {
    using ::iqsm::ops::Transaction;

    template<Facet Meta>
    class modifier {
    public:
      using Id = typename Aspect<Meta>::Id;
      using Quantum = typename Aspect<Meta>::Quantum;
      using Item = typename Aspect<Meta>::Item;

      modifier(Transaction& transaction, Id id) : transaction(transaction), id(id), original(required_item(transaction.current, id)), value(*original) {}
      ~modifier() { apply(); }
      modifier(const modifier&) = delete;
      modifier& operator=(const modifier&) = delete;
      modifier(modifier&& other) noexcept : transaction(other.transaction), id(other.id), original(other.original), value(std::move(other.value)), dirty(other.dirty), applied(other.applied) { other.applied = true; }
      modifier& operator=(modifier&&) = delete;
      Quantum* operator->() { dirty = true; return &value; }
      const Quantum* operator->() const { return &value; }
      Quantum& operator*() { dirty = true; return value; }
      const Quantum& operator*() const { return value; }

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

    template<Facet Meta>
    class creator_xion {
    public:
      using Id = typename Aspect<Meta>::Id;
      using Quantum = typename Aspect<Meta>::Quantum;

      explicit creator_xion(Transaction& transaction) : transaction(transaction) {}
      Id operator()(Quantum value);

    private:
      Transaction& transaction;
    };

    template<Facet Meta>
    class creator_quark {
    public:
      using Id = typename Aspect<Meta>::Id;
      using Quantum = typename Aspect<Meta>::Quantum;

      creator_quark(Transaction& transaction, Id id) : transaction(transaction), id(id) {}
      Id operator()(Quantum value);

    private:
      Transaction& transaction;
      Id id;
    };
} // namespace iqsm::detail::ops::particle

namespace iqsm::ops::particle {
  template<Facet Meta>
  auto get(World world, typename Aspect<Meta>::Id id) -> typename Aspect<Meta>::Item {
    required(world, "ops::particle::get(): world");
    const auto field = world->field<Meta>();
    if (not field->container.contains(id)) { throw std::runtime_error(std::format("ops::particle::get(): missing entity: {}", id)); }
    const auto item = field->container.at(id);
    required(item, "ops::particle::get(): item");
    return item;
  }

  template<Facet Meta>
  auto get_opt(World world, typename Aspect<Meta>::Id id) -> std::optional<typename Aspect<Meta>::Item> {
    required(world, "ops::particle::get_opt(): world");
    const auto field = world->field<Meta>();
    if (not field->container.contains(id)) { return std::nullopt; }
    return field->container.at(id);
  }

  template<Facet Meta>
  bool contains(World world, typename Aspect<Meta>::Id id) {
    required(world, "ops::particle::contains(): world");
    return world->field<Meta>()->container.contains(id);
  }
} // namespace iqsm::ops::particle

namespace iqsm::ops::particle {
  template<Facet Meta>
  auto modify(Transaction& transaction, typename Aspect<Meta>::Id id) { return detail::ops::particle::modifier<Meta>(transaction, id); }

  template<Facet Meta>
  auto create(Transaction& transaction) requires XionFacet<Meta> { return detail::ops::particle::creator_xion<Meta>(transaction); }

  template<Facet Meta>
  auto create(Transaction& transaction, typename Aspect<Meta>::Id id) requires (!XionFacet<Meta>) { return detail::ops::particle::creator_quark<Meta>(transaction, id); }

  template<Facet Meta>
  void remove(Transaction& transaction, typename Aspect<Meta>::Id id) {
    auto fd = std::make_shared<delta::FieldDiff<Meta>>();
    fd->deleted = fd->deleted.insert(id);

    auto wd = std::make_shared<delta::WorldState>();
    wd->fields = wd->fields.insert(
        Aspect<Meta>::typeId,
        std::static_pointer_cast<const delta::FieldDiffAbstract>(freeze(fd)));

    transaction.absorb(freeze(wd));
  }

} // namespace iqsm::ops::particle

namespace iqsm::detail::ops::particle {
  template<Facet Meta>
  typename modifier<Meta>::Item modifier<Meta>::required_item(World world, const Id& id) {
    required(world, "modifier: world");
    const auto field = world->field<Meta>();
    if (not field->container.contains(id)) { throw std::runtime_error(std::format("modifier: missing entity: {}", id)); }
    const auto item = field->container.at(id);
    required(item, "modifier: item");
    return item;
  }

  template<Facet Meta>
  void modifier<Meta>::apply() {
    if (applied) return;
    applied = true;
    if (!dirty) return;

    auto fd = std::make_shared<delta::FieldDiff<Meta>>();
    fd->changed = fd->changed.insert(id, typename delta::FieldDiff<Meta>::Change{
        .before = original,
        .after = Aspect<Meta>::create(std::move(value)),
    });

    auto wd = std::make_shared<delta::WorldState>();
    wd->fields = wd->fields.insert(
        Aspect<Meta>::typeId,
        std::static_pointer_cast<const delta::FieldDiffAbstract>(freeze(fd)));

    transaction.absorb(freeze(wd));
  }

  template<Facet Meta>
  typename creator_xion<Meta>::Id creator_xion<Meta>::operator()(Quantum value) {
    const auto id = Id::generate_random();

    auto fd = std::make_shared<delta::FieldDiff<Meta>>();
    fd->added = fd->added.insert(id, Aspect<Meta>::create(std::move(value)));

    auto wd = std::make_shared<delta::WorldState>();
    wd->fields = wd->fields.insert(
        Aspect<Meta>::typeId,
        std::static_pointer_cast<const delta::FieldDiffAbstract>(freeze(fd)));

    transaction.absorb(freeze(wd));
    return id;
  }

  template<Facet Meta>
  typename creator_quark<Meta>::Id creator_quark<Meta>::operator()(Quantum value) {
    auto fd = std::make_shared<delta::FieldDiff<Meta>>();
    fd->added = fd->added.insert(id, Aspect<Meta>::create(std::move(value)));

    auto wd = std::make_shared<delta::WorldState>();
    wd->fields = wd->fields.insert(
        Aspect<Meta>::typeId,
        std::static_pointer_cast<const delta::FieldDiffAbstract>(freeze(fd)));

    transaction.absorb(freeze(wd));
    return id;
  }
} // namespace iqsm::detail::ops::particle

