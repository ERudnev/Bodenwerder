#include "_common.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <format>
#include <vector>

#include <base/logging.h>
#include <fQSM/api/interface.h>

// Synthetic frame workload, shaped like the Eltanin frame after the frame-phases change:
//   input    one Writing: the window input snapshot (two vectors), the clock, the cameras drive their nodes
//   simulate one Stewarding: every body moves in place (direct), a slice of things changes through the patch,
//            a few composites spawn and a few expire (Thing with custody of a Body and a Node; the Node holds
//            a Mesh with a release reaction; the Node is in a group), the world global advances
//   render   one Writing: the viewport
//   end      one Writing: the window frame counter
// Population stays constant. Reports the time per phase and per frame. Numbers are for comparison between
// commits on one machine; the asserts pin the population and the reaction counts.
namespace {
    using namespace fqsm::api;
    using Stopwatch = std::chrono::steady_clock;

    double elapsed_us(Stopwatch::time_point since) {
        return std::chrono::duration<double, std::micro>(Stopwatch::now() - since).count();
    }

    namespace sim {
        struct Vec3 {
            double x = 0, y = 0, z = 0;
        };

        struct Window : Entity<Window> {
            struct Snapshot {
                std::vector<bool> keys = std::vector<bool>(512);
                std::vector<float> axes = std::vector<float>(8);
            };
            struct Quantum {
                Snapshot previous;
                Snapshot current;
                integer frame = 0;
            };
        };
        struct Ticker : Entity<Ticker> {
            struct Quantum {
                int64 absolute = 0;
            };
        };
        struct Root : Entity<Root> {
            struct Quantum {};
        };
        struct Node : Entity<Node> {
            struct Quantum {
                Vec3 position;
                float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            };
        };
        struct Node_group : Group<Node_group, Root, Node> {};
        struct Camera : Feature<Camera, Node> {
            struct Quantum {
                float moveScale = 1.0f;
            };
        };
        struct Mesh : Feature<Mesh, Node> {
            struct Quantum {
                integer surfaces = 3;
            };
            static const Behavior customAspectReactions();
        };
        struct Body : Entity<Body> {
            struct Quantum {
                Vec3 position;
                Vec3 velocity;
                float mass = 1.0f;
            };
        };
        struct Thing : Entity<Thing> {
            struct Quantum {
                Custody<Body> body;
                Custody<Node> node;
                int64 bornAt = 0;
                integer hits = 0;
            };
            static const Behavior customAspectReactions();
        };
        struct Viewport : Entity<Viewport> {
            struct Quantum {
                integer width = 1920;
                integer height = 1080;
                float clear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            };
        };
        struct Runtimes : Entity<Runtimes> {
            struct Quantum {};
            struct Global {
                integer released = 0;
            };
        };
        struct World : Entity<World> {
            struct Quantum {};
            struct Global {
                int64 step = 0;
                bool paused = false;
            };
        };

        void release(Retrospecting context, Mesh::Id, const Mesh::Quantum&) {
            with<Runtimes>::modify_global(context)->released += 1;
        }
        auto Mesh::customAspectReactions() -> const Behavior {
            return {
                reaction::deletion<Mesh>(&release),
            };
        }
        auto Thing::customAspectReactions() -> const Behavior {
            return {
                reaction::structural::custody<Thing, Body, &Thing::Quantum::body>{},
                reaction::structural::custody<Thing, Node, &Thing::Quantum::node>{},
            };
        }

        Schema schema() {
            return ask::schema::merge({
                ask::schema::aspect<Window>(),
                ask::schema::aspect<Ticker>(),
                ask::schema::aspect<Root>(),
                ask::schema::aspect<Node>(),
                ask::schema::aspect<Node_group>(),
                ask::schema::aspect<Camera>(),
                ask::schema::aspect<Mesh>(),
                ask::schema::aspect<Body>(),
                ask::schema::aspect<Thing>(),
                ask::schema::aspect<Viewport>(),
                ask::schema::aspect<Runtimes>(),
                ask::schema::aspect<World>(),
            });
        }

        // knobs: change one, rebuild, compare (the asserts follow them)
        constexpr int things_alive = 2000;
        constexpr int frames = 300;
        constexpr int churn_per_frame = 4;       // composites spawned and expired per frame
        constexpr int touched_per_frame = 100;   // things modified through the patch per frame
        constexpr bool hot_pass = true;          // bodies moved in place through direct<> (taints Body)
        constexpr bool in_group = true;          // a spawned Node joins the root group
        constexpr double dt = 1.0 / 60.0;

        // a composite: a Thing holding a Body and a Node; the Node carries a Mesh and sits in the root group
        Thing::Id spawn(Writing context, Root::Id root, int64 now, double seed) {
            const auto node = in_group
                ? with<Node_group>::addElement(context, root, Node::Quantum{.position = {seed, 0.0, 0.0}})
                : with<Node>::create(context, Node::Quantum{.position = {seed, 0.0, 0.0}});
            with<Mesh>::extend(context, node, {});
            const auto body = with<Body>::create(context, {.position = {seed, 0.0, 0.0}, .velocity = {1.0, 0.0, 0.0}});
            return with<Thing>::create(context, {.body = body, .node = node, .bornAt = now});
        }

        struct Fixture {
            Root::Id root;
            Window::Id window;
            Ticker::Id ticker;
            Viewport::Id viewport;
            std::vector<Node::Id> cameras;
            std::deque<Thing::Id> things;
        };

        struct Phases {
            double input = 0, simulate = 0, render = 0, end = 0;
            // inside simulate: the work, then the session end (normalization and integration)
            double hot = 0, touch = 0, spawn = 0, expire = 0, commit = 0;
        };


        void frame(establish::Realm& world, Fixture& f, int index, Phases& phases) {
            const int64 now = static_cast<int64>(index) * 16'667;

            const auto t0 = Stopwatch::now();
            {
                Writing input = world;
                {
                    auto window = with<Window>::modify(input, f.window);
                    window->previous = window->current;
                    const std::size_t key = static_cast<std::size_t>(index * 7) % 512;
                    window->current.keys[key] = not window->current.keys[key];
                    window->current.axes[static_cast<std::size_t>(index) % 8] = static_cast<float>(index);
                }
                with<Ticker>::modify(input, f.ticker)->absolute = now;
                for (const auto camera : f.cameras)
                    with<Node>::modify(input, camera)->position.x += 0.01;
            }
            phases.input += elapsed_us(t0);

            const auto t1 = Stopwatch::now();
            {
                Stewarding simulate = world;
                auto mark = Stopwatch::now();
                if constexpr (hot_pass) {
                    auto bodies = simulate.direct<Body>();
                    for (auto [id, body] : bodies.items) {
                        body.position.x += body.velocity.x * dt;
                        body.position.y += body.velocity.y * dt;
                        body.position.z += body.velocity.z * dt;
                    }
                }
                phases.hot += elapsed_us(mark); mark = Stopwatch::now();
                for (int i = 0; i < touched_per_frame; ++i) {
                    const auto id = f.things[static_cast<std::size_t>((index * touched_per_frame + i) % things_alive)];
                    with<Thing>::modify(simulate, id)->hits += 1;
                }
                phases.touch += elapsed_us(mark); mark = Stopwatch::now();
                for (int i = 0; i < churn_per_frame; ++i)
                    f.things.push_back(spawn(simulate, f.root, now, static_cast<double>(index)));
                phases.spawn += elapsed_us(mark); mark = Stopwatch::now();
                for (int i = 0; i < churn_per_frame; ++i) {
                    with<Thing>::remove(simulate, f.things.front());
                    f.things.pop_front();
                }
                with<World>::modify_global(simulate)->step += 1;
                phases.expire += elapsed_us(mark);
                phases.commit -= elapsed_us(t1);   // the session end is what remains of the phase
            }
            phases.commit += elapsed_us(t1);
            phases.simulate += elapsed_us(t1);

            const auto t2 = Stopwatch::now();
            {
                Writing render = world;
                with<Viewport>::modify(render, f.viewport)->clear[0] = static_cast<float>(index % 2);
            }
            phases.render += elapsed_us(t2);

            const auto t3 = Stopwatch::now();
            {
                Writing end = world;
                with<Window>::modify(end, f.window)->frame = index;
            }
            phases.end += elapsed_us(t3);
        }
    }
}

namespace tests {

void workload_frame()
{
    using namespace sim;

    establish::Realm world(schema());
    Fixture f = world.branch([&](Writing context) {
        Fixture built{
            .root = with<Root>::create(context, {}),
            .window = with<Window>::create(context, {}),
            .ticker = with<Ticker>::create(context, {}),
            .viewport = with<Viewport>::create(context, {}),
        };
        with<Node_group>::extend(context, built.root);
        for (int i = 0; i < 2; ++i) {
            const auto node = with<Node>::create(context, {});
            with<Camera>::extend(context, node, {});
            built.cameras.push_back(node);
        }
        for (int i = 0; i < things_alive; ++i)
            built.things.push_back(spawn(context, built.root, 0, static_cast<double>(i)));
        return built;
    });
    EXPECT_TRUE(world.result().good());
    EXPECT_EQ(with<Thing>::count(world), static_cast<std::size_t>(things_alive));

    Phases phases;
    const auto start = Stopwatch::now();
    for (int index = 0; index < frames; ++index) {
        frame(world, f, index, phases);
        EXPECT_TRUE(world.result().good());
    }
    const double total_us = elapsed_us(start);

    EXPECT_EQ(with<Thing>::count(world), static_cast<std::size_t>(things_alive));
    EXPECT_EQ(with<Body>::count(world), static_cast<std::size_t>(things_alive));
    EXPECT_EQ(with<Mesh>::count(world), static_cast<std::size_t>(things_alive));
    EXPECT_EQ(with<Node>::count(world), static_cast<std::size_t>(things_alive + 2));
    EXPECT_EQ(with<Node_group>::get(world, f.root).size(), static_cast<std::size_t>(in_group ? things_alive : 0));
    EXPECT_EQ(with<Runtimes>::get_global(world).released, static_cast<integer>(frames * churn_per_frame));
    EXPECT_EQ(with<World>::get_global(world).step, static_cast<int64>(frames));
    EXPECT_EQ(with<Window>::get(world, f.window).frame, static_cast<integer>(frames - 1));

    const auto per_frame = [&](double us) { return us / frames; };
    base::message(std::format(
        "workload_frame: {} frames, {} things, {} bodies moved in place, {} touched, {} spawned and {} expired per frame",
        frames, things_alive, things_alive, touched_per_frame, churn_per_frame, churn_per_frame));
    base::message(std::format(
        "workload_frame: per frame {:.1f} us = input {:.1f} + simulate {:.1f} + render {:.1f} + end {:.1f} (4 transactions)",
        per_frame(total_us), per_frame(phases.input), per_frame(phases.simulate), per_frame(phases.render), per_frame(phases.end)));
    base::message(std::format(
        "workload_frame: simulate = hot pass {:.1f} + touch {:.1f} + spawn {:.1f} + expire {:.1f} + session end {:.1f} us",
        per_frame(phases.hot), per_frame(phases.touch), per_frame(phases.spawn), per_frame(phases.expire), per_frame(phases.commit)));
}

} // namespace tests
