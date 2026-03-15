#pragma once

#include <format>
#include <stdexcept>
#include <utility>

#include <iQSM/_forwards.h>
#include <iQSM/meta.h>
#include <iQSM/delta.h>
#include <iQSM/field.h>
#include <iQSM/operations/transaction.h> // rmove after "iqsm/repo/" evolved to replace old "Transaction/Context"

namespace iqsm::ops::particle {
  template<meta::Particle Meta>
  auto modifier(Context context, Id<Meta> id);

  template<meta::Entity Meta>
  auto create(Context context, Quantum<Meta> value) -> Id<Meta>;

  template<meta::Quark Meta>
  auto create(Context context, Id<Meta> id, Quantum<Meta> value) -> Id<Meta>;

  template<meta::Particle Meta>
  void remove(Context context, Id<Meta> id);

  template<meta::Particle Meta>
  auto get(World world, Id<Meta> id) -> const Quantum<Meta>&;

  template<meta::Particle Meta>
  auto item(World world, Id<Meta> id) -> typename Facet<Meta>::Item;

  template<meta::Particle Meta>
  bool exists(World world, Id<Meta> id);

} // namespace iqsm::ops::particle

namespace iqsm::detail::ops::particle {
    using ::iqsm::ops::Context;

    template<meta::Particle Meta>
    class modifier {
    public:
      using Id = iqsm::Id<Meta>;
      using Quantum = iqsm::Quantum<Meta>;
      using Item = typename Facet<Meta>::Item;

      modifier(Context context, Id id)
          : context(context)
          , id(id)
          , original(required_item(context.world, id))
          , value(*original)
          , dirty(true)
      {}
      ~modifier() { apply(); }
      modifier(const modifier&) = delete;
      modifier& operator=(const modifier&) = delete;
      modifier(modifier&& other) noexcept : context(other.context), id(other.id), original(other.original), value(std::move(other.value)), dirty(other.dirty), applied(other.applied) { other.applied = true; }
      modifier& operator=(modifier&&) = delete;
      Quantum* operator->() { dirty = true; return &value; }
      Quantum& operator*() { dirty = true; return value; }

    private:
      static Item required_item(World world, const Id& id);
      void apply();

      Context context;
      Id id;
      Item original;
      Quantum value;
      bool dirty = false;
      bool applied = false;
    };

} // namespace iqsm::detail::ops::particle

namespace iqsm::ops::particle {
  template<meta::Particle Meta>
  auto get(World world, Id<Meta> id) -> const Quantum<Meta>& {
    const auto field = world->field<Meta>();
    if (not field->container.contains(id)) { throw std::runtime_error(std::format("ops::particle::get(): missing entity: {}", id)); }
    const auto item = field->container.at(id);
    return *item;
  }

  template<meta::Particle Meta>
  auto item(World world, Id<Meta> id) -> typename Facet<Meta>::Item {
    const auto field = world->field<Meta>();
    if (not field->container.contains(id)) { throw std::runtime_error(std::format("ops::particle::item(): missing entity: {}", id)); }
    return field->container.at(id);
  }

  template<meta::Particle Meta>
  bool exists(World world, Id<Meta> id) {
    return world->field<Meta>()->container.contains(id);
  }
} // namespace iqsm::ops::particle

namespace iqsm::ops::particle {
  template<meta::Particle Meta>
  auto modifier(Context context, Id<Meta> id) { return detail::ops::particle::modifier<Meta>(context, id); }

  template<meta::Entity Meta>
  auto create(Context context, Quantum<Meta> value) -> Id<Meta> {
    const auto id = Id<Meta>::generate_random();

    auto fd = base::make_shared<delta::FieldDiff<Meta>>();
    auto op = typename delta::FieldDiff<Meta>::Operation{};
    op.add = Facet<Meta>::create(std::move(value));
    fd->ops = fd->ops.insert(id, std::move(op));

    auto wd = base::make_shared<delta::Fields>();
    wd->fields = wd->fields.insert(Facet<Meta>::typeId, freeze(fd));

    context.absorb(freeze(wd));
    return id;
  }

  template<meta::Quark Meta>
  auto create(Context context, Id<Meta> id, Quantum<Meta> value) -> Id<Meta> {
    auto fd = base::make_shared<delta::FieldDiff<Meta>>();
    auto op = typename delta::FieldDiff<Meta>::Operation{};
    op.add = Facet<Meta>::create(std::move(value));
    fd->ops = fd->ops.insert(id, std::move(op));

    auto wd = base::make_shared<delta::Fields>();
    wd->fields = wd->fields.insert(Facet<Meta>::typeId, freeze(fd));

    context.absorb(freeze(wd));
    return id;
  }

  template<meta::Particle Meta>
  void remove(Context context, Id<Meta> id) {
    auto fd = base::make_shared<delta::FieldDiff<Meta>>();
    auto op = typename delta::FieldDiff<Meta>::Operation{};
    op.remove = true;
    fd->ops = fd->ops.insert(id, std::move(op));

    auto wd = base::make_shared<delta::Fields>();
    wd->fields = wd->fields.insert(
        Facet<Meta>::typeId,
        freeze(fd));

    context.absorb(freeze(wd));
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

    context.absorb(freeze(wd));
  }

} // namespace iqsm::detail::ops::particle

