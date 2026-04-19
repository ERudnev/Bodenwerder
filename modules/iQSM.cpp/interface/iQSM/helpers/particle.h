#pragma once

#include <format>
#include <functional>
#include <stdexcept>
#include <utility>

#include <iQSM/_forwards.h>
#include <iQSM/delta.h>
#include <iQSM/field.h>
#include <iQSM/world.h>
#include <iQSM/internals/delta_builders.h>
#include <iQSM/internals/fields_mutable.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/meta/facade.h>
#include <iQSM/repository/transactions/accumulator.h>
#include <iQSM/repository/transactions/once.h>
#include <iQSM/repository/transactions/quantum.h>
#include <iQSM/repository/transactions/staged.h>

namespace iqsm::helpers::particle {
  template<meta::Particle Meta>
  auto modifier(Writing writing, Id<Meta> id);

  template<meta::HasQuantum Meta>
  bool equal(const Quantum<Meta>& a, const Quantum<Meta>& b);

  template<meta::Entity Meta>
  auto create(Writing writing, Quantum<Meta> value) -> Id<Meta>;

  template<meta::Quark Meta>
  auto create(Writing writing, Id<Meta> id, Quantum<Meta> value) -> Id<Meta>;

  template<meta::Particle Meta>
  void remove(Writing writing, Id<Meta> id);

  template<meta::Particle Meta>
  auto get(Reading world, Id<Meta> id) -> const Quantum<Meta>&;

  template<meta::Particle Meta>
  auto item(Reading world, Id<Meta> id) -> Item<Meta>;

  template<meta::Particle Meta>
  bool exists(Reading world, Id<Meta> id);

  // Mass operation: run the same per-item callable for every id in Field<Meta> (ids from reading snapshot);
  // each call receives the same repo object and the forwarded trailing arguments.
  template<meta::Particle Meta, typename F, typename... Args>
  void massop(Writing writing, F&& op, Args&&... args);

} // namespace iqsm::helpers::particle

namespace iqsm::helpers::particle {
  template<meta::Particle Meta>
  auto get(Reading world, Id<Meta> id) -> const Quantum<Meta>& {
    const auto field = world->field<Meta>();
    if (not field->container.contains(id)) { throw std::runtime_error(std::format("helpers::particle::get(): missing entity: {}", id)); }
    const auto item = field->container.at(id);
    return *item;
  }

  template<meta::Particle Meta>
  auto item(Reading world, Id<Meta> id) -> Item<Meta> {
    const auto field = world->field<Meta>();
    if (not field->container.contains(id)) { throw std::runtime_error(std::format("helpers::particle::item(): missing entity: {}", id)); }
    return field->container.at(id);
  }

  template<meta::Particle Meta>
  bool exists(Reading world, Id<Meta> id) {
    return world->field<Meta>()->container.contains(id);
  }
} // namespace iqsm::helpers::particle

namespace iqsm::helpers::particle {
  template<meta::Particle Meta>
  auto modifier(Writing writing, Id<Meta> id) {
    return repo::Quantum<Meta>(writing, id);
  }

  template<meta::Entity Meta>
  auto create(Writing writing, Quantum<Meta> value) -> Id<Meta> {
    const auto id = Id<Meta>::generate_random();
    repo::Once(writing).submit(internals::delta::make_atomic<Meta>(id, std::nullopt, base::make_shared<const Quantum<Meta>>(std::move(value))));
    return id;
  }

  template<meta::Quark Meta>
  auto create(Writing writing, Id<Meta> id, Quantum<Meta> value) -> Id<Meta> {
    repo::Once(writing).submit(internals::delta::make_atomic<Meta>(id, std::nullopt, base::make_shared<const Quantum<Meta>>(std::move(value))));
    return id;
  }

  template<meta::Particle Meta>
  void remove(Writing writing, Id<Meta> id) {
    repo::Staged staged{writing};
    const auto field = staged->field<Meta>();
    if (not field->container.contains(id)) { throw std::runtime_error(std::format("helpers::particle::remove(): missing entity: {}", id)); }
    const auto before = field->container.at(id);
    
    staged.remove<Meta>(id, before);
  }

  template<meta::Particle Meta, typename F, typename... Args>
  void massop(Writing writing, F&& op, Args&&... args) {
    repo::Accumulator seq{writing};
    for (const auto& kv : seq->field<Meta>()->container) {
      std::invoke(std::forward<F>(op), seq, kv.first, std::forward<Args>(args)...);
    }
  }

} // namespace iqsm::helpers::particle

