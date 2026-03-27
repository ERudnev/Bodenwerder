#pragma once

#include <format>
#include <optional>
#include <stdexcept>

#include <iQSM/_forwards.h>
#include <iQSM/delta.h>
#include <iQSM/meta/aspect_id.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/meta/facade.h>
#include <iQSM/repository/commit.h>

namespace iqsm::helpers::handle {
    // Handle instances live in World like other particles.
    // Use `helpers::particle::{item,get,exists,...}` out of the box.
    // This helper only provides a world-writing constructor-like operation.
    template<meta::Handle Meta>
    auto declare(repo::Commit commit, Quantum<Meta> value) -> Id<Meta>;
}

namespace iqsm::helpers::handle {
    template<meta::Handle Meta>
    auto declare(repo::Commit commit, Quantum<Meta> value) -> Id<Meta> {
        const auto id = Id<Meta>::generate_random();

        auto fd = base::make_shared<delta::FieldDiff<Meta>>();
        auto op = typename delta::FieldDiff<Meta>::Operation{};
        op.after = base::make_shared<const Quantum<Meta>>(std::move(value));
        fd->ops.emplace(id, std::move(op));

        auto wd = base::make_shared<delta::Fields>();
        wd->fields.emplace(types::aspectId<Meta>(), freeze(fd));

        commit.push(freeze(wd));
        return id;
    }
}

