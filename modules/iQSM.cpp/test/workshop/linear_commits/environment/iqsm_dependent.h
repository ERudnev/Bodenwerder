#pragma once

#include "iqsm_root.h"
#include "../repository/permit.h"
#include "../repository/transaction.h"

// usings on repo::
namespace iqsm_mock {
    using Reading = repo::Reading;
    using Writing = repo::Permit;
}


namespace iqsm_mock::helpers {
    using namespace iqsm_mock::repo;


    struct RaiiHelperCore : Transaction {
        explicit RaiiHelperCore(Writing writing, EntityId) : repo::Transaction(writing) {}
        void absorb(Delta) override { base::message("RaiiHelperCore::absorb"); }
        
        Entity* operator->() { static Entity entity{}; return &entity; }

        void finish() override {
          head.upstream(internals::make_delta_from_one_entity(value));
          disconnect();
        }

        Entity value;
    };

    long reader(Reading reading, EntityId) {
        base::message("helpers::reader(Reading,EntityId)");
        return reading.use_count();
    }

    RaiiHelperCore modifier(Writing writing, EntityId id) { base::message("helpers::modifier(Writing,EntityId)"); return RaiiHelperCore(std::move(writing), id); }
    const Entity& get(Reading, EntityId) { static const Entity entity{}; return entity; }

}
