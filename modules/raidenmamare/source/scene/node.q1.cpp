#include <rmmr/scene/node.q1.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace rmmr::scene {

    using namespace fqsm::api;

    namespace {

        auto make_transform(const Node::Quantum& quantum) -> mat4 {
            return glm::translate(mat4{1.0f}, quantum.pose.position) * glm::mat4_cast(glm::normalize(quantum.pose.rotation));
        }

    } // namespace

    auto Node::Actions::create(Writing context, Locator locator) -> Id {
        return Node::BaseActions::create(context, Node::Quantum{.pose = Pose::from(locator)});
    }

    auto Node::Actions::transform(Reading context, Id id) -> mat4 {
        return make_transform(with<Node>::get(context, id));
    }

}
