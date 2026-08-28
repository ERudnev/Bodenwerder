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

    auto Node::Actions::create(Writing context, Pose pose) -> Id {
        return Node::BaseActions::create(context, Node::Quantum{.pose = pose, .visible = true});
    }

    auto Node::Actions::transform(Reading context, Id id) -> mat4 {
        return make_transform(with<Node>::get(context, id));
    }

    void Node::Actions::setVisible(Writing context, Id id, bool visible) {
        with<Node>::modify(context, id)->visible = visible;
    }

}
