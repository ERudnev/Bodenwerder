#pragma once

#include <format>
#include <stdexcept>
#include <utility>

#include <iQSM/_forwards.h>
#include <iQSM/meta.h>
#include <iQSM/delta.h>
#include <iQSM/field.h>
#include <iQSM/internals/delta_builders.h>
#include <iQSM/repository/commit.h>

namespace iqsm::ops::particle {
  template<meta::Particle Meta>
  auto modifier(repo::Commit commit, Id<Meta> id);

  template<meta::Entity Meta>
  auto create(repo::Commit commit, Quantum<Meta> value) -> Id<Meta>;

  template<meta::Quark Meta>
  auto create(repo::Commit commit, Id<Meta> id, Quantum<Meta> value) -> Id<Meta>;

  template<meta::Particle Meta>
  void remove(repo::Commit commit, Id<Meta> id);

  template<meta::Particle Meta>
  auto get(World world, Id<Meta> id) -> const Quantum<Meta>&;

  template<meta::Particle Meta>
  auto item(World world, Id<Meta> id) -> typename Facet<Meta>::Item;

  template<meta::Particle Meta>
  bool exists(World world, Id<Meta> id);

} // namespace iqsm::ops::particle

namespace iqsm::detail::ops::particle {
    template<meta::Particle Meta>
    class modifier {
    public:
      using Id = iqsm::Id<Meta>;
      using Quantum = iqsm::Quantum<Meta>;
      using Item = typename Facet<Meta>::Item;

      modifier(repo::Commit commit, Id id)
          : commit(std::move(commit))
          , id(id)
          , original(required_item(this->commit.initial, id))
          , value(*original)
          , dirty(true)
      {}
      ~modifier() { apply(); }
      modifier(const modifier&) = delete;
      modifier& operator=(const modifier&) = delete;
      modifier(modifier&& other) noexcept : commit(std::move(other.commit)), id(other.id), original(other.original), value(std::move(other.value)), dirty(other.dirty), applied(other.applied) { other.applied = true; }
      modifier& operator=(modifier&&) = delete;
      Quantum* operator->() { dirty = true; return &value; }
      Quantum& operator*() { dirty = true; return value; }

    private:
      static Item required_item(World world, const Id& id);
      void apply();

      repo::Commit commit;
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
  auto modifier(repo::Commit commit, Id<Meta> id) { return detail::ops::particle::modifier<Meta>(std::move(commit), id); }

  template<meta::Entity Meta>
  auto create(repo::Commit commit, Quantum<Meta> value) -> Id<Meta> {
    const auto id = Id<Meta>::generate_random();

    commit.push(internals::delta::make_atomic<Meta>(id, Facet<Meta>::create(std::move(value)), std::nullopt, false));
    return id;
  }

  template<meta::Quark Meta>
  auto create(repo::Commit commit, Id<Meta> id, Quantum<Meta> value) -> Id<Meta> {
    commit.push(internals::delta::make_atomic<Meta>(id, Facet<Meta>::create(std::move(value)), std::nullopt, false));
    return id;
  }

  template<meta::Particle Meta>
  void remove(repo::Commit commit, Id<Meta> id) {
    commit.push(internals::delta::make_atomic<Meta>(id, std::nullopt, std::nullopt, true));
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

    commit.push(internals::delta::make_atomic<Meta>(id, std::nullopt, std::pair<Item, Item>{ original, Facet<Meta>::create(std::move(value)) }, false));
  }

} // namespace iqsm::detail::ops::particle

