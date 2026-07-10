#include <Raidenmamare/scene/node.q1.h>

#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace rmmr::scene {

    using namespace fqsm::api;

    namespace {

        auto rotation_from_hpb(HPB hpb) -> quat {
            const vec3 radians = glm::radians(hpb);
            const glm::vec3 glm_euler{
                radians.y,
                radians.x,
                radians.z,
            };
            return glm::normalize(quat{glm_euler});
        }

        auto make_transform(const Node::Quantum& quantum) -> mat4 {
            const auto translate = glm::translate(mat4{1.0f}, quantum.position);
            const auto rotation = glm::mat4_cast(glm::normalize(quantum.rotation));
            return translate * rotation;
        }

        auto heading_pitch_bank_from_rotation(quat rotation) -> vec3 {
            const glm::vec3 euler = glm::eulerAngles(glm::normalize(rotation));
            return vec3{euler.y, euler.x, euler.z};
        }

    } // namespace

    auto Node::Actions::create(Writing context, Locator locator) -> Id {
        return with<Node>::create(context, Node::Quantum{
            .position = locator.pos,
            .rotation = rotation_from_hpb(locator.euler),
        });
    }

    auto Node::Actions::transform(Reading context, Id id) -> mat4 {
        return make_transform(with<Node>::get(context, id));
    }

    auto Node::Actions::hpb(Reading context, Id id) -> HPB {
        const vec3 radians = heading_pitch_bank_from_rotation(with<Node>::get(context, id).rotation);
        return glm::degrees(radians);
    }

    void Node::Actions::hpb(Writing context, Id id, HPB value) {
        with<Node>::modify(context, id)->rotation = rotation_from_hpb(value);
    }

}
