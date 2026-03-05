#pragma once

#include <optional>

#include <iQSM/_forwards.h>
#include <iQSM/meta.h>
#include <iQSM/delta.h>
#include <iQSM/field.h>

namespace iqsm::ops::cache {
  template<meta::Aspect Meta, typename RecomputeOne>
  Delta recompute(World world, RecomputeOne recompute_one);
}

namespace iqsm::ops::cache {
  template<meta::Aspect Meta, typename RecomputeOne>
  Delta recompute(World world, RecomputeOne recompute_one) {
    required(world, "ops::cache::recompute(): world");
    if (not world->schema->aspects.contains(Facet<Meta>::typeId)) return nullptr;

    const auto field = world->field<Meta>();
    auto fd = std::make_shared<delta::FieldDiff<Meta>>();

    for (const auto& kv : field->container) {
      const auto& id = kv.first;
      const auto& item = kv.second;

      auto updated = recompute_one(world, id, *item);
      if (not updated) continue;

      fd->changed = fd->changed.insert(id, typename delta::FieldDiff<Meta>::Change{
          .before = item,
          .after = Facet<Meta>::create(std::move(*updated)),
      });
    }

    if (fd->added.empty() and fd->changed.empty() and fd->deleted.empty()) return nullptr;

    auto out = std::make_shared<delta::WorldState>();
    out->fields = out->fields.insert(
        Facet<Meta>::typeId,
        std::static_pointer_cast<const delta::FieldDiffAbstract>(freeze(fd)));

    return freeze(out);
  }
}

