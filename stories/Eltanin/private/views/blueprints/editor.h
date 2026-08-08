#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <base/maybe.h>
#include <base/types/common_types.h>
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
#include "mech/semantics/space.h"
#include "mech/semantics/subframe.h"

#include <fQSM/api/interface.h>

namespace eltanin::views {

    using namespace fqsm::api;

    // Blueprint editor view. Optional persist via blueprint::Loader::save (contentAutoSave in editor.cpp).
    // Own scene; Game swaps product views when `open`.
    struct Blueprints {
        struct Layers {
            bool plate;
            bool frame;
            bool inner;
        };

        // What the scenicAlias points at in Asset.data.
        enum class Source : std::uint8_t {
            plate,    // hull[index]
            inner,    // cells[index] inner volume mesh
            corner,   // cells[index].corners[sub]
            halfEdge, // cells[index].edges[sub] + pole
        };

        enum class FloorFilter : std::uint8_t {
            all,
            onlyCurrent,
            notAbove,
        };

        struct Actor {
            rmmr::scene::actor::Mesh::Id id;
            mech::layer layer;
            Source source;
            std::size_t index; // cells / hull
            std::size_t sub;   // corners / edges; 0 for plate/inner
            mech::subframe::halfEdge::Pole pole; // halfEdge only
            int floor; // lattice pose.pos.y
        };

        struct State {
            base::maybe<rmmr::scene::Root::Id> scene;
            base::maybe<rmmr::scene::Camera::Id> camera;
            base::maybe<rmmr::scene::Grid::Id> grid;
            base::maybe<rmmr::scene::actor::Mesh::Id> worldCursor;
            std::vector<resource::blueprint::Asset::Id> loaded;
            base::maybe<resource::blueprint::Asset::Id> hovered;
            std::vector<rmmr::renderer::Integer32> selection;
            Layers layers;
            int currentFloor;
            FloorFilter floorFilter;
            base::common_types::index3 cursorLattice;
            std::map<int, std::vector<rmmr::scene::actor::Mesh::Id>> floors;
            std::vector<Actor> levelOne; // drawn: frame pieces / inner / plate
            std::vector<Actor> levelTwo; // stash; unused for now
            struct {
                base::maybe<Actor> target; // change existing; empty + place = create at cursorLattice
                bool place;
                bool close;
            } spaceMenu;
        };

        State state;

        void create(Writing, filepath directory);
        void show(Writing, resource::blueprint::Asset::Id);
        void syncVisuals(Writing); // rebuild actors from hovered Asset.data
        void deleteSelection(Writing); // mutate data + save + syncVisuals
        void rotateSelection(Writing, const std::vector<mech::orient::key>& turn); // ±90° via orient::turn*; keep selection
        void persistHovered(Writing); // Loader::save for hovered asset
        auto spawnFromPack(Writing, rmmr::resource::meshpack::Asset::Id, const std::string& entry, const rmmr::Pose&, mech::layer, Source, std::size_t index, std::size_t sub, mech::subframe::halfEdge::Pole, int floor, rmmr::RGB albedo, float opacity) -> base::maybe<Actor>;
        void applyLayers(Writing);
        void syncGridToFloor(Writing);
        void updateWorldCursor(Writing, rmmr::renderer::Integer32 under);
        void draw(Writing, bool& open);
        void bindView(std::vector<rmmr::wrapper::Product::View>& views, bool open, const rmmr::wrapper::Product::View& world_view) const;
    };

}
