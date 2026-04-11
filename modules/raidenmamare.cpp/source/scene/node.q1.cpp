#include <Raidenmamare/scene/node.q1.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace rmmr::scene {
    struct Node_private : Node::Operations {
        static auto make_transform(const Quantum& quantum) -> mat4 {
            const auto translation = glm::translate(mat4{1.0f}, quantum.translation);
            const auto rotation = glm::mat4_cast(glm::normalize(quantum.rotation));
            return translation * rotation;
        }
    };

    auto Node::Operations::transform(Reading world, Id id) -> mat4 {
        return Node_private::make_transform(ops::particle::get<Node>(world, id));
    }

    const Invariants Node::invariants{
        .structural = {},
        .logical = {},
    };
}
