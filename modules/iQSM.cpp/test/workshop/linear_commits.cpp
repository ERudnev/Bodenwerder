#include "linear_commits/environment/iqsm_dependent.h"

#include "linear_commits/repository/permit.h"
#include "linear_commits/repository/sequence.h"
#include "linear_commits/repository/accumulator.h"
#include "linear_commits/repository/staged.h"
#include "linear_commits/repository/branch.h"

namespace user_code {

    using namespace iqsm_mock;
    using namespace iqsm_mock::repo;

 
    long clientSideReader(repo::Reading commit, EntityId id) {
        base::message("handbook::clientSideReader()");
        return helpers::reader(commit, id);
    }

    void almostAtomicOp(repo::Writing commit, EntityId id) {
        base::message("notebook::almostAtomicOp(Writing,EntityId)");
        if (not helpers::reader(commit, id)) return;
        auto mod = helpers::modifier(commit, id);
        mod->value = mod->value + 1;
    }

    void midLevelSequenceScope(repo::Writing commit, EntityId id) {
        base::message("notebook::midLevelSequenceScope(Writing,EntityId)");
        helpers::reader(commit, id);
        repo::Branch master{commit};
        repo::Sequence seq{master};
        almostAtomicOp(seq, id);
        master.rebase(seq);
    }

    /*
    void midLevelAccumulatorScope(repo::Writing commit, EntityId id) {
        base::message("notebook::midLevelAccumulatorScope(Writing,EntityId)");
        repo::Accumulator acc{commit};
        almostAtomicOp(acc, id);
        almostAtomicOp(acc, id);
        // no push(), rely on RAII / ~Accumulator -> finish
    }*/
}

namespace tests {

    void linear_commits() {
        using namespace user_code;
        using namespace iqsm_mock;

        // create quasi-world as just world (not recommended, btw)
        const auto schema = std::make_shared<SchemaTag>();
        const auto world = std::make_shared<WorldTag>();

        repo::Branch master{world};
        repo::Branch subtask{repo::Writing{master}};
        midLevelSequenceScope(subtask, {});
    }

}
