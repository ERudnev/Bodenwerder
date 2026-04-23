#include "_common.h"

#include <chrono>
#include <format>
#include <string>
#include <type_traits>
#include <vector>

#include "multistate/model.q1.h"

using namespace iqsm::q1_gateway;
using namespace RnD;

namespace {

repo::Branch make_master_branch() {
    const auto pairingSchema = ops::schema::assemble<Logic::Pairing, View::HappyHouse>();
    return repo::Branch(ops::world::create_no_resources(pairingSchema));
}

struct TimeMark {
    std::chrono::steady_clock::duration sinceStart{};
    std::chrono::steady_clock::duration segment{};
    std::string what;
};

struct State {
    std::chrono::steady_clock::time_point testStart{};
    std::chrono::steady_clock::time_point clockNow{};

    repo::Branch master;
    int houseCount{};
    std::vector<RnD::Logic::House::Id> houseIds;
    std::vector<RnD::Logic::Pairing::Id> pairingIds;
    std::vector<TimeMark> marks;

    explicit State(int houseCount_)
        : master(make_master_branch())
        , houseCount(houseCount_)
    {}

    void setup() {
        houseIds.clear();
        pairingIds.clear();
        houseIds.reserve(houseCount);
        pairingIds.reserve(houseCount / 2);
        marks.clear();
        marks.reserve(8);

        testStart = std::chrono::steady_clock::now();
        clockNow = testStart;

        marks.push_back({
            std::chrono::steady_clock::duration::zero(),
            std::chrono::steady_clock::duration::zero(),
            "test started (local t = 0)",
        });
    }

    void logStep(std::string what) {
        clockNow = std::chrono::steady_clock::now();
        const auto sinceStart = clockNow - testStart;
        const auto segment = marks.empty()
            ? std::chrono::steady_clock::duration{}
            : sinceStart - marks.back().sinceStart;
        marks.push_back({sinceStart, segment, std::move(what)});
    }

    void logReports() const {
        base::message("");
        base::message("immutable_containers_perf2 — timeline (steady clock, from empty world after `master`)");
        base::message(std::format("{:-<100}", ""));
        base::message(std::format("{:<59} {:>30} ms {:>20} ms", "event", "started", "duration"));

        const auto ms = [](std::chrono::steady_clock::duration d) -> double {
            return std::chrono::duration<double, std::milli>(d).count();
        };

        for (const auto& mark : marks) {
            base::message(std::format(
                "{:<59} {:>30.3f} ms {:>20.3f} ms",
                mark.what,
                ms(mark.sinceStart),
                ms(mark.segment)
            ));
        }
        base::message(std::format("{:-<100}", ""));
    }
};

void createHouses(State& state) {
    {
        repo::Accumulator accumulator{state.master};
        for (int pairBase = 0; pairBase < state.houseCount; pairBase += 2) {
            const auto firstHouseId = ops::particle::create<RnD::Logic::House>(accumulator, RnD::Logic::House::Quantum{
                .happiness = integer{pairBase},
                .dynamics = integer{0},
            });

            const auto secondHouseId = ops::particle::create<RnD::Logic::House>(accumulator, RnD::Logic::House::Quantum{
                .happiness = integer{pairBase + 1},
                .dynamics = integer{0},
            });
            state.houseIds.push_back(firstHouseId);
            state.houseIds.push_back(secondHouseId);
            state.pairingIds.push_back(ops::particle::create<RnD::Logic::Pairing>(accumulator, RnD::Logic::Pairing::Quantum{
                .participants = {firstHouseId, secondHouseId},
            }));
        }
    }

    state.logStep(std::format("created {} houses (each with a pairing)", state.houseCount));
}

void runTailRemovalPasses(State& state, int pairingsPerPass, int removalPassCount) {
    const auto removeFirstHouseFromTailPairings = [&](Writing parent, int passIndex) {
        repo::Accumulator nested{parent};

        const int pairingCount = state.houseCount / 2;
        const int tailStart = pairingCount - pairingsPerPass * (passIndex + 1);

        for (int k = 0; k < pairingsPerPass; ++k) {
            const RnD::Logic::Pairing::Id pairingId = state.pairingIds[tailStart + k];
            const auto firstHouseId = ops::particle::get<RnD::Logic::Pairing>(nested, pairingId).participants.first;
            ops::particle::remove<RnD::Logic::House>(nested, firstHouseId);
        }
    };

    const auto pairingCountInWorld = [](Reading world) -> int {
        return static_cast<int>(world->field<RnD::Logic::Pairing>()->container.size());
    };

    for (int pass = 0; pass < removalPassCount; ++pass) {
        removeFirstHouseFromTailPairings(state.master, pass);
        const int pairingsAfterPass = pairingCountInWorld(state.master);
        const int expectedPairings = state.houseCount / 2 - (pass + 1) * pairingsPerPass;
        EXPECT_EQ(pairingsAfterPass, expectedPairings);
    }

    state.logStep(std::format("{}x removal — first house from Pairing (tail)", removalPassCount));
}

void validateLinks(State& state) {
    const Reading world = state.master;

    std::vector<RnD::Logic::House::Id> kept;
    std::vector<RnD::Logic::House::Id> lost;
    kept.reserve(state.houseIds.size());
    lost.reserve(state.houseIds.size());

    for (const auto id : state.houseIds) {
        if (ops::particle::exists<RnD::Logic::House>(world, id)) {
            kept.push_back(id);
        } else {
            lost.push_back(id);
        }
    }

    state.houseIds = std::move(kept);
    state.logStep(std::format("validate links: dropped {}", lost.size()));
}

void massPairingUpdates(State& state, int rounds) {
    for (int r = 0; r < rounds; ++r) {
        ops::particle::massop<RnD::Logic::Pairing>(
            state.master,
            &RnD::Logic::Pairing::Operations::update);
    }
    state.logStep(std::format("mass pairing update: {} rounds (massop each round)", rounds));
}

template<typename TransactionT>
void updateHousesQuadratic(State& state) {
    TransactionT tx{state.master};

    for (const auto& outer : tx->template field<Logic::House>()->container) {
        const auto outerId = outer.first;

        integer sumOthers = 0;
        integer countOthers = 0;
        for (const auto& inner : tx->template field<Logic::House>()->container) {
            if (inner.first == outerId) {
                continue;
            }
            sumOthers += ops::particle::get<Logic::House>(tx, inner.first).happiness;
            ++countOthers;
        }
        if (countOthers == 0) {
            continue;
        }

        const integer avgOthers = sumOthers / countOthers;
        const integer cur = ops::particle::get<Logic::House>(tx, outerId).happiness;
        const integer blended = (cur + avgOthers) / 2;
        ops::particle::modifier<Logic::House>(tx, outerId)->happiness = blended;
    }

    const char* tag = [] {
        if constexpr (std::is_same_v<TransactionT, repo::Accumulator>) {
            return "acc";
        } else if constexpr (std::is_same_v<TransactionT, repo::Branch>) {
            return "branch";
        } else if constexpr (std::is_same_v<TransactionT, repo::Sequence>) {
            return "sequence";
        } else {
            return "tx";
        }
    }();
    state.logStep(std::format("n²: mean of others per house, blend halfway ({})", tag));
}

template<typename TransactionT>
void shuffleHouses(State& state) {
    TransactionT tx{state.master};

    auto rng = [&seed = state.houseCount]() mutable -> uint32_t {
        // xorshift32: fast, deterministic, good enough for a perf shuffle.
        uint32_t x = static_cast<uint32_t>(seed) ^ 0x9E3779B9u;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        seed = static_cast<int>(x);
        return x;
    };

    for (const auto& kv : tx->template field<Logic::House>()->container) {
        const auto id = kv.first;
        const integer cur = ops::particle::get<Logic::House>(tx, id).happiness;
        const integer delta = static_cast<integer>(static_cast<int>(rng() % 5u) - 2); // [-2..+2]
        ops::particle::modifier<Logic::House>(tx, id)->happiness = cur + delta;
    }

    state.logStep("shuffle houses: happiness += rand([-2..+2])");
}

void logTotalHouseHappiness(State& state) {
    const Reading world = state.master;
    integer sum = 0;
    for (const auto& kv : world->field<Logic::House>()->container) {
        sum += ops::particle::get<Logic::House>(world, kv.first).happiness;
    }
    state.logStep(std::format("total happiness (all houses): {}", sum));
}

// sugar, garbage
using pairingsPerPass = int;
using removalPassCount = int;
using massPairingUpdateRounds = int;

} // namespace

namespace tests {
    void immutable_containers_perf2() {        

        State state(5000);
        state.setup();

        createHouses(state);
        runTailRemovalPasses(state, pairingsPerPass(100), removalPassCount(10));
        validateLinks(state);
        massPairingUpdates(state, massPairingUpdateRounds(10));
        shuffleHouses<repo::Accumulator>(state);
        updateHousesQuadratic<repo::Accumulator>(state);
        shuffleHouses<repo::Branch>(state);
        updateHousesQuadratic<repo::Branch>(state);
        shuffleHouses<repo::Sequence>(state);
        updateHousesQuadratic<repo::Sequence>(state);
        logTotalHouseHappiness(state);
        state.logReports();
    }
}
