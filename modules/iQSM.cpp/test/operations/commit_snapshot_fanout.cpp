#include <cstdlib>
#include <functional>
#include <memory>
#include <utility>
#include <base/_include.interface.h>
#include <base/logging.h>
// notebook / R&D draft
// intentionally not a real compilable test

namespace test_domain {

    struct EntityId {
    };

    struct Entity {
        int value;
    };
}

namespace iqsm_fork {

    using World = std::shared_ptr<void>;
    using Delta = std::shared_ptr<void>;

    namespace repo {
        using Reading = World;

        struct Commit;
        struct Writing;
        struct Interface;
        struct Branch;
        struct Sequence;
        struct Accumulator;

        struct Commit {
            Commit(const Commit&) = delete;
            Commit(Commit&&) = default;
            operator Reading() const { return reading(); }
            ~Commit() { base::message("-commit"); }

        private:
            Commit(World world, std::function<void(Delta)> sink) : initial(std::move(world)), receiver(std::move(sink)) {
                base::message("+commit");
            }
            Reading reading() const { return initial; }
            void consume(Delta delta) const { receiver(std::move(delta)); }

            friend struct Interface;

            World initial;
            std::function<void(Delta)> receiver;
        };

        struct Writing {
            Writing(const Writing&) = delete;
            Writing(Writing& other) noexcept : stolen(std::move(other.stolen)) { base::message("Writing::Writing(Writing&)"); }
            Writing(Writing&&) = default;

            Writing(Commit& commit) : stolen(std::move(commit)) { base::message("Writing::Writing(Commit&)"); }
            Writing(Interface& anyRepoObject);

            operator Reading() const { return static_cast<Reading>(stolen); }

        private:
            explicit Writing(Commit&& commit) : stolen(std::move(commit)) { base::message("Writing::Writing(Commit&&)"); }

            Commit stolen;

            friend struct Interface;
        };

        struct Interface {
            Interface(const Interface&) = delete;
            Interface(Interface&&) = delete;
            virtual ~Interface() = default;
            virtual operator Commit() = 0;

        protected:
            explicit Interface(Reading) {}
            explicit Interface(Writing) {}
            static Reading readingOf(const Writing& writing) { return static_cast<Reading>(writing); }
            static void consume(Writing writing, Delta delta) { writing.stolen.consume(std::move(delta)); }
            static Commit createCommit(Reading reading, std::function<void(Delta)> receiver) { return Commit{std::move(reading), std::move(receiver)}; }
        };

        struct Branch : Interface {
            explicit Branch(Reading reading) : Interface(std::move(reading)) {}
            explicit Branch(Writing writing) : Interface(std::move(writing)) {}
            operator Reading() const { return {}; }
            operator Commit() override { base::message("Branch::operator Commit()"); return createCommit({}, [](Delta) {}); }
        };

        struct Sequence : Interface {
            explicit Sequence(Writing writing) : Interface(std::move(writing)) {}
            operator Reading() const { return {}; }
            operator Commit() override { base::message("Sequence::operator Commit()"); return createCommit({}, [](Delta) {}); }
            void push() {}
        };

        struct Accumulator : Interface {
            explicit Accumulator(Writing writing) : Interface(std::move(writing)) {}
            operator Reading() const { return {}; }
            operator Commit() override { base::message("Accumulator::operator Commit()"); return createCommit({}, [](Delta) {}); }
            void push() {}
        };

        inline Writing::Writing(Interface& anyRepoObject) : stolen(static_cast<Commit>(anyRepoObject)) { base::message("Writing::Writing(Interface&)"); }
    }

    namespace helpers {

        using namespace iqsm_fork::repo;

        struct RaiiHelperCore : repo::Interface {
            explicit RaiiHelperCore(Writing writing, test_domain::EntityId) : repo::Interface(std::move(writing)) {}
            ~RaiiHelperCore() = default;
            operator Commit() override { base::message("RaiiHelperCore::operator Commit()"); return createCommit({}, [](Delta) {}); }
            test_domain::Entity* operator->() { static test_domain::Entity entity{}; return &entity; }
        };

        RaiiHelperCore modifier(Writing writing, test_domain::EntityId id) { base::message("helpers::modifier(Writing,EntityId)"); return RaiiHelperCore(std::move(writing), id); }
        const test_domain::Entity& get(Reading, test_domain::EntityId) { static const test_domain::Entity entity{}; return entity; }

        long reader(Reading reading, test_domain::EntityId) {
            base::message("helpers::reader(Reading,EntityId)");
            return reading.use_count();
        }
    }
}

namespace notebook {

    using namespace iqsm_fork;
    using namespace iqsm_fork::helpers;
    using namespace test_domain;

    long clientSideReader(repo::Reading commit, EntityId id){
        base::message("handbook::clientSideReader()");
        return helpers::reader(commit, id);
    }

    void lowLevelAtomicOp(repo::Writing commit, EntityId id) {
        base::message("notebook::lowLevelAtomicOp(Writing,EntityId)");
        auto mod = helpers::modifier(commit, id);
        mod->value = mod->value + 1;
    }

    void almostAtomicOp(repo::Writing commit, EntityId id) {
        base::message("notebook::almostAtomicOp(Writing,EntityId)");
        if (not reader(commit, id)) return;
        auto mod = helpers::modifier(commit, id);
        mod->value = mod->value + 1;
    }

    void midLevelSequenceScope(repo::Writing commit, EntityId id) {
        base::message("notebook::midLevelSequenceScope(Writing,EntityId)");
        reader(commit, id);
        repo::Sequence seq{commit};
        lowLevelAtomicOp(seq, id);
        almostAtomicOp(seq, id);
        lowLevelAtomicOp(seq, id);
        seq.push();
    }

    void midLevelAccumulatorScope(repo::Writing commit, EntityId id) {
        base::message("notebook::midLevelAccumulatorScope(Writing,EntityId)");
        repo::Accumulator acc{commit};
        lowLevelAtomicOp(acc, id);
        lowLevelAtomicOp(acc, id);
        acc.push();
    }

}

namespace tests {
    void commit_snapshot_fanout() {
        base::message("commit_snapshot_fanout: begin");
        iqsm_fork::repo::Branch branch{iqsm_fork::repo::Reading{iqsm_fork::World{}}};
        notebook::lowLevelAtomicOp(branch, {});
        notebook::clientSideReader(branch, {});
        //notebook::midLevelSequenceScope(branch, {});
        //base::message("all done");
        //notebook::midLevelAccumulatorScope(branch, {});
    }
}
