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

namespace iqsm::helpers::resource {
    template<meta::Resource Meta>
    auto declare(repo::Commit commit, Quantum<Meta> value) -> Id<Meta>;

    template<meta::Resource Meta>
    auto get(World world, Id<Meta> id) -> const Quantum<Meta>&;

    template<meta::Resource Meta>
    bool exists(World world, Id<Meta> id);
}

namespace iqsm::helpers::resource {
    template<meta::Resource Meta>
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

    template<meta::Resource Meta>
    auto get(World world, Id<Meta> id) -> const Quantum<Meta>& {
        const auto field = world->field<Meta>();
        if (not field->container.contains(id)) { throw std::runtime_error(std::format("ops::resource::get(): missing instance: {}", id)); }
        const auto item = field->container.at(id);
        return *item;
    }

    template<meta::Resource Meta>
    bool exists(World world, Id<Meta> id) {
        return world->field<Meta>()->container.contains(id);
    }
}

