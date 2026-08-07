#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <base/maybe.h>
#include <eltanin/resources/blueprint.q1.h>
#include <rmmr/renderer/types.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/gizmos.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/wrapper/product.h>

#include "mech/blueprint.h"
#include "mech/semantics/layers.h"

#include <fQSM/api/interface.h>

namespace eltanin::views {

    using namespace fqsm::api;

    // Blueprint editor view: in-memory only (never writes .blueprint files).
    // Own scene; Game swaps product views when `open`.
    struct Blueprints {
        struct Layers {
            bool plate;
            bool frame; // also toggles wing stubs
            bool inner;
        };

        enum class Source : std::uint8_t {
            cell,
            stub,
            plate,
        };

        struct Actor {
            rmmr::scene::actor::Mesh::Id id;
            mech::layer layer;
            Source source;
            std::size_t index; // into Asset.data.cells / stubs / hull
            int floor; // lattice pose.pos.y
        };

        struct State {
            base::maybe<rmmr::scene::Root::Id> scene;
            base::maybe<rmmr::scene::Camera::Id> camera;
            base::maybe<rmmr::scene::Grid::Id> grid;
            std::vector<resource::blueprint::Asset::Id> loaded;
            base::maybe<resource::blueprint::Asset::Id> hovered;
            std::vector<rmmr::renderer::Integer32> selection;
            Layers layers;
            std::map<int, bool> floorVisible; // UI; rebuilt keys on syncVisuals
            std::map<int, std::vector<rmmr::scene::actor::Mesh::Id>> floors;
            std::vector<Actor> levelOne; // drawn: frame / inner / plate / wing
            std::vector<Actor> levelTwo; // stash; unused for now
            struct {
                base::maybe<Actor> target;
                bool close;
            } spaceMenu;
        };

        State state;

        void create(Writing, filepath directory);
        void show(Writing, resource::blueprint::Asset::Id);
        void syncVisuals(Writing); // rebuild actors from hovered Asset.data
        void deleteSelection(Writing); // mutate data + syncVisuals; never disk
        auto spawnFromPack(Writing, rmmr::resource::meshpack::Asset::Id, const std::string& entry, const mech::Pose&, mech::layer, Source, std::size_t index, rmmr::RGB albedo, float opacity) -> base::maybe<Actor>;
        void applyLayers(Writing);
        void draw(Writing, bool& open);
        void bindView(std::vector<rmmr::wrapper::Product::View>& views, bool open, const rmmr::wrapper::Product::View& world_view) const;
    };

}
