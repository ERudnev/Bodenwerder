#pragma once

#include <memory>
#include <base/logging.h>

namespace iqsm_mock {

    // Fork-level carriers (see test/operations/commit_snapshot_fanout.cpp notebook):
    // repo objects and repo::Commit close over these; they do not depend on Branch/Sequence/etc.
    struct SchemaTag{};
    using Schema = std::shared_ptr<SchemaTag>;

    struct WorldTag {
        Schema schema{};
    };
    struct DeltaTag {};

    using World = std::shared_ptr<WorldTag>;
    using Delta = std::shared_ptr<DeltaTag>;

    struct EntityId {
    };

    struct Entity {
        int value{};
    };

    namespace internals {

        // mirrors iqsm::detail::lifecycle::pre_remove_action_into_accumulator (topology for future typed merge)
        struct FieldsMutable {
            // coarse stand-in for "fields non-empty" (typed map in master FieldsMutable)
            unsigned pending_ops = 0;

            bool empty() const { return pending_ops == 0; }

            void absorb(Schema schema, Delta rhs) {
                base::message("FieldsMutable::absorb()");
                (void)schema;
                (void)rhs;
                if (rhs)
                    ++pending_ops;
            }

            // mirrors iqsm::internals::FieldsMutable::snapshot
            Delta snapshot(Schema schema) const {
                base::message("FieldsMutable::snapshot()");
                (void)schema;
                return std::make_shared<DeltaTag>();
            }

            // mirrors iqsm::internals::FieldsMutable::push
            Delta push() {
                base::message("FieldsMutable::push()");
                pending_ops = 0;
                return std::make_shared<DeltaTag>();
            }

            // Staged / notebook topology stubs (master: template add_op<Meta> / remove / update / set_global)
            void add_entity(EntityId id, Entity after) {
                base::message("FieldsMutable::add_entity");
                (void)id;
                (void)after;
                ++pending_ops;
            }

            void remove_entity(EntityId id, Entity before) {
                base::message("FieldsMutable::remove_entity");
                (void)id;
                (void)before;
                ++pending_ops;
            }

            void update_entity(EntityId id, Entity before, Entity after) {
                base::message("FieldsMutable::update_entity");
                (void)id;
                (void)before;
                (void)after;
                ++pending_ops;
            }

            void set_global_stub() {
                base::message("FieldsMutable::set_global");
                ++pending_ops;
            }
        };

        inline void pre_remove_action_into_staging(World snapshot, FieldsMutable& staging, EntityId id, Entity before) {
            base::message("pre_remove_action_into_staging(snapshot, staging, id, before)");
            (void)staging;
            (void)snapshot;
            (void)id;
            (void)before;
        }

        static Delta make_delta_from_one_entity(Entity) { return std::make_shared<DeltaTag>(); }
    }

    namespace operations {
        static World integrate(World in, Delta) {
            base::message("integration...");
            return in;
        }

        static World validate_smart(World before, World now) {
            base::message("validation...");
            (void)before;
            return now;
        }

        // mirrors iqsm::operations::make_delta (integration.h / integration.cpp)
        static Delta make_delta(World from, World to) {
            base::message("make_delta(from,to)...");
            (void)from;
            (void)to;
            return std::make_shared<DeltaTag>();
        }
    }
}
