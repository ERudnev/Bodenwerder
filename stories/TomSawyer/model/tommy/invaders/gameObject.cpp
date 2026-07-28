#include <tommy/invaders/gameObject.h>

#include <algorithm>

namespace tommy::invaders {

    using namespace fqsm::api;

    auto GameObject::Actions::takeDamage(Writing context, Id id, integer amount) -> integer {
        if (not with<GameObject>::exists(context, id) or amount <= 0) {
            return with<GameObject>::exists(context, id)
                ? with<GameObject>::get(context, id).hitpoints
                : 0;
        }
        auto object = with<GameObject>::modify(context, id);
        object->hitpoints = std::max(integer{0}, object->hitpoints - amount);
        return object->hitpoints;
    }

    auto GameObject::Actions::alive(Reading context, Id id) -> bool {
        return with<GameObject>::exists(context, id)
            and with<GameObject>::get(context, id).hitpoints > 0;
    }

    auto GameObject::customAspectReactions() -> const Behavior {
        return {};
    }

}
