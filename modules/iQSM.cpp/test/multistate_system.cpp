#include "_common.h"

#include <functional>
#include <vector>

#include <base/shared_reference.h>

#include <iQSM/api/_gateway.h>
#include <iQSM/repository/agents/collaboration.h>
#include <iQSM/repository/agents/subsystem.h>

#include "multistate/model.q1.h"

//temp includes (will go to iQSM implementation of library types invented here)
#include <iQSM/meta/aspect_id.h>


namespace tests {

    using namespace iqsm::q1_gateway;
    using namespace iqsm::agents;

    // will go to the iQSM later...
    namespace experimental {

        using namespace RnD;        

        struct Server : Subsystem {
            static auto definedSchema() -> iqsm::Schema {
                static const auto schema = ops::schema::assemble<Logic::Pairing>();
                return schema;
            }

            iqsm::repo::Branch main{ops::world::create_no_resources(definedSchema())};

            auto schema() const -> iqsm::Schema override { return definedSchema(); }
            auto access() -> Update override {
                return Update{
                    .current = main,
                    .replace = [this](iqsm::Reading next) {
                        main.rebase(next);
                    },
                };
            }

            // placeholder "logic"
            void startup() {
                repo::Accumulator tx{main};
                const auto first = ops::particle::create<Logic::House>(tx, Logic::House::Quantum{
                    .happiness = integer{10},
                    .dynamics = integer{0},
                });
                const auto second = ops::particle::create<Logic::House>(tx, Logic::House::Quantum{
                    .happiness = integer{20},
                    .dynamics = integer{0},
                });

                ops::particle::create<Logic::Pairing>(tx, Logic::Pairing::Quantum{{first, second}});
            }

            void updateWorld() {
                ops::particle::massop<Logic::Pairing>(main, &Logic::Pairing::Operations::update);
            }
        };

        struct Renderer : Subsystem {
            static auto definedSchema() -> iqsm::Schema {
                static const auto schema = ops::schema::assemble<View::HappyHouse>();
                return schema;
            }

            iqsm::repo::Branch main{ops::world::create_no_resources(definedSchema())};

            auto schema() const -> iqsm::Schema override { return definedSchema(); }
            auto access() -> Update override {
                return Update{
                    .current = main,
                    .replace = [this](iqsm::Reading next) {
                        main.rebase(next);
                    },
                };
            }

            void updateWorld() {
                ops::particle::massop<View::HappyHouse>(main, &View::HappyHouse::Operations::update);
            }
        };        
    }

    // local test helpers:

void multistate_system() {
    const iqsm::ref<experimental::Server> serverObj = base::make_shared<experimental::Server>();
    const iqsm::ref<experimental::Renderer> rendererObj = base::make_shared<experimental::Renderer>();

    experimental::Server& server = *serverObj;
    experimental::Renderer& renderer = *rendererObj;

    server.startup();
    iqsm::agents::Collaboration collaborator(serverObj, rendererObj);

    const auto pairingId = server.main->field<RnD::Logic::Pairing>()->container.begin()->first;
    const auto firstHouseId = ops::particle::get<RnD::Logic::Pairing>(server.main, pairingId).participants.first;

    // Bootstrap: renderer learns shared state from server.
    collaborator.sync();
    EXPECT_EQ(ops::particle::get<RnD::Logic::House>(renderer.main, firstHouseId).happiness, integer{10});
    EXPECT_EQ(ops::particle::get<RnD::Logic::House>(renderer.main, firstHouseId).dynamics, integer{0});
    EXPECT_EQ(ops::particle::get<RnD::View::HappyHouse>(renderer.main, firstHouseId).previous_happiness, integer{10});

    // Left-only change arrives to renderer.
    server.updateWorld();
    collaborator.sync();
    EXPECT_EQ(ops::particle::get<RnD::Logic::House>(server.main, firstHouseId).happiness, integer{12});
    EXPECT_EQ(ops::particle::get<RnD::Logic::House>(renderer.main, firstHouseId).happiness, integer{12});
    EXPECT_EQ(ops::particle::get<RnD::View::HappyHouse>(renderer.main, firstHouseId).previous_happiness, integer{10});

    // Right-only change pushes renderer-side interpretation back into shared overlap.
    renderer.updateWorld();
    collaborator.sync();
    EXPECT_EQ(ops::particle::get<RnD::Logic::House>(server.main, firstHouseId).happiness, integer{12});
    EXPECT_EQ(ops::particle::get<RnD::Logic::House>(server.main, firstHouseId).dynamics, integer{2});
    EXPECT_EQ(ops::particle::get<RnD::View::HappyHouse>(renderer.main, firstHouseId).previous_happiness, integer{12});

    // Concurrent changes before sync resolve as right-wins for shared House field.
    base::message("[multistate_system] expecting 2 House merge conflicts in concurrent sync section");
    server.updateWorld();
    renderer.updateWorld();
    collaborator.sync();
    EXPECT_EQ(ops::particle::get<RnD::Logic::House>(server.main, firstHouseId).happiness, integer{12});
    EXPECT_EQ(ops::particle::get<RnD::Logic::House>(server.main, firstHouseId).dynamics, integer{0});
    EXPECT_EQ(ops::particle::get<RnD::View::HappyHouse>(renderer.main, firstHouseId).previous_happiness, integer{12});
    base::message("[multistate_system] concurrent conflict section finished");

    // Server-only non-overlap change must not replace renderer world.
    ops::particle::remove<RnD::Logic::Pairing>(server.main, pairingId);
    collaborator.sync();
    const iqsm::Reading renderer_head = renderer.main;
    EXPECT_EQ(renderer.access().current, renderer_head);

    // Regression: collaboration must not "bootstrap-wipe" a valid side when the other side is empty,
    // and neither side changed since collaboration construction.
    {
        using namespace RnD;

        struct Side : Subsystem {
            static auto definedSchema() -> iqsm::Schema {
                static const auto schema = ops::schema::assemble<Logic::House>();
                return schema;
            }

            iqsm::repo::Branch main{ops::world::create_no_resources(definedSchema())};

            auto schema() const -> iqsm::Schema override { return definedSchema(); }
            auto access() -> Update override {
                struct Replacer {
                    iqsm::repo::Branch* main;
                    void operator()(iqsm::Reading next) const { main->rebase(next); }
                };

                return Update{
                    .current = main,
                    .replace = Replacer{&main},
                };
            }
        };

        const iqsm::ref<Side> left = base::make_shared<Side>();
        const iqsm::ref<Side> right = base::make_shared<Side>();

        repo::Accumulator tx{right->main};
        const auto house = ops::particle::create<Logic::House>(tx, Logic::House::Quantum{
            .happiness = integer{7},
            .dynamics = integer{0},
        });
        tx.complete();

        iqsm::agents::Collaboration bootstrap(left, right);
        bootstrap.sync();

        // This currently reproduces the bug: right loses its house after first sync.
        EXPECT_TRUE(ops::particle::exists<Logic::House>(right->main, house));
    }

}

} // namespace tests
