#pragma once

#include <optional>
#include <type_traits>

#include <iQSM/_forwards.h>
#include <iQSM/meta.h>
#include <iQSM/delta.h>
#include <iQSM/field.h>

namespace iqsm::ops::cache {
  template<meta::Aspect Meta, auto UpdateOne>
  Delta update(World world);
}

namespace iqsm::ops::cache {
  template<meta::Aspect Meta, auto UpdateOne>
  Delta update(World world) {
    using Id = typename Facet<Meta>::Id;
    using Item = typename Facet<Meta>::Item;

    static_assert(std::is_invocable_r_v<Item, decltype(UpdateOne), World, Id>);

    if (not world->schema->aspects.contains(Facet<Meta>::typeId)) return ::iqsm::delta::empty();

    const auto field = world->field<Meta>();
    auto fd = base::make_shared<delta::FieldDiff<Meta>>();

    for (const auto& kv : field->container) {
      const auto& id = kv.first;
      const auto& before = kv.second;

      const auto after = UpdateOne(world, id);
      if (after == before) continue;

      fd->changed = fd->changed.insert(id, typename delta::FieldDiff<Meta>::Change{
          .before = before,
          .after = after,
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

