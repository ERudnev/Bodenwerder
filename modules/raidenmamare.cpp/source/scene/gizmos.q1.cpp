#include <rmmr/scene/gizmos.q1.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    auto Grid::Actions::create(Writing context, Pos position, HPB hpb, Grid::Quantum quantum) -> Id {
        const auto node = Node::Actions::create(context, Locator{
            .pos = position,
            .euler = hpb,
        });
        with<Grid>::extend(context, node, std::move(quantum));
        return node;
    }

    void Grid::Actions::submit(Reading context, Id node, renderer::CommandBuffer& where) {
        const auto& grid = with<Grid>::get(context, node);
        where.push_back(renderer::Command{
            .pass = renderer::Pass::gizmo,
            .model = Node::Actions::transform(context, node),
            .geometry = grid.geometry,
            .material = grid.material,
            .albedo = RGB{0.0f, 0.0f, 0.0f},
            .opacity = grid.opacity,
            .instance_data = {},
            .instance_count = renderer::Count{1},
            .render_state = {},
        });
    }

}
