#pragma once

#include <type_traits>
#include <stdexcept>
#include <utility>

#include <iQSM/_forwards.h>
#include <iQSM/meta.h>
#include <iQSM/delta.h>
#include <iQSM/field.h>
#include <iQSM/repository/commit.h>

namespace iqsm::ops::global {
    template<meta::Aspect Meta>
    auto get(World world) -> typename Facet<Meta>::Global {
        const auto field = world->field<Meta>();
        return field->global;
    }

    template<meta::Aspect Meta>
    auto modifier(repo::Commit commit);
}

namespace iqsm::detail::ops::global {
    template<meta::Aspect Meta>
    class modifier {
    public:
        using GlobalData = typename Facet<Meta>::GlobalData;
        using Global = typename Facet<Meta>::Global;

        explicit modifier(repo::Commit commit)
            : commit(std::move(commit))
            , original(required_item(this->commit.initial))
            , value(*original)
        {}

        ~modifier() { apply(); }
        modifier(const modifier&) = delete;
        modifier& operator=(const modifier&) = delete;
        modifier(modifier&& other) noexcept
            : commit(std::move(other.commit))
            , original(other.original)
            , value(std::move(other.value))
            , dirty(other.dirty)
            , applied(other.applied)
        { other.applied = true; }
        modifier& operator=(modifier&&) = delete;

        GlobalData* operator->() { dirty = true; return &value; }
        GlobalData& operator*() { dirty = true; return value; }

    private:
        static Global required_item(World world) {
            if (not world->schema->aspects.contains(Facet<Meta>::typeId)) {
                throw std::runtime_error("ops::global::modifier(): aspect is not in schema");
            }
            return world->field<Meta>()->global;
        }

        void apply() {
            if (applied) return;
            applied = true;
            if (!dirty) return;

            auto field_delta = base::make_shared<delta::FieldDiff<Meta>>();
            field_delta->global_change = std::pair<typename delta::FieldDiff<Meta>::Global, typename delta::FieldDiff<Meta>::Global>{
                original,
                base::make_shared<const GlobalData>(std::move(value)),
            };

            auto world_delta = base::make_shared<delta::Fields>();
            world_delta->fields = world_delta->fields.insert(
                Facet<Meta>::typeId,
                freeze(field_delta));

            commit.push(freeze(world_delta));
        }

        repo::Commit commit;
        Global original;
        GlobalData value;
        bool dirty = false;
        bool applied = false;
    };
}

namespace iqsm::ops::global {
    template<meta::Aspect Meta>
    auto modifier(repo::Commit commit) {
        return detail::ops::global::modifier<Meta>(std::move(commit));
    }
}

