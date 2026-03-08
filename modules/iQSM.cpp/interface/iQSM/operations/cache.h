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
    if (not world->schema->aspects.contains(Facet<Meta>::typeId)) return ::iqsm::delta::empty();

    const auto field = world->field<Meta>();
    auto fd = base::make_shared<delta::FieldDiff<Meta>>();

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

    if (fd->added.empty() and fd->changed.empty() and fd->deleted.empty()) return ::iqsm::delta::empty();

    auto out = base::make_shared<delta::Fields>();
    out->fields = out->fields.insert(
        Facet<Meta>::typeId,
        freeze(fd));

    return freeze(out);
  }
}

