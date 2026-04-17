#pragma once

#include "transaction.h"

namespace iqsm_mock::repo {

    // Скоуп-буфер: много мелких правок -> одна Delta при выходе из блока.
    // Мастер: repo::Staged(Commit&) + commit.push(staged.push()); здесь — Transaction& и скрытый Commit через flush_staged_delta -> receive.
    struct Staged final {
        Transaction& owner;
        internals::FieldsMutable staged{};

        explicit Staged(Transaction& t) : owner(t) {}

        ~Staged() {
            base::message("~Staged");
            if (not staged.empty())
                owner.flush_staged_delta(staged.push());
        }

        void add(EntityId id, Entity after) { staged.add_entity(id, std::move(after)); }

        void remove(EntityId id, Entity before) {
            internals::pre_remove_action_into_staging(static_cast<Reading>(owner), staged, id, before);
            staged.remove_entity(id, std::move(before));
        }

        void update(EntityId id, Entity before, Entity after) {
            staged.update_entity(id, std::move(before), std::move(after));
        }

        void set_global() { staged.set_global_stub(); }
    };

}
